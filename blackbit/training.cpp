#include "training.hpp"

#include "compare_engines.hpp"
#include "engine.hpp"
#include "eval.hpp"
#include "experiment_framework.hpp"
#include "rules.hpp"
#include "search_result_info.hpp"
#include "training_features.hpp"

#include "bee/file_reader.hpp"
#include "bee/file_writer.hpp"
#include "bee/format_map.hpp"
#include "bee/format_optional.hpp"
#include "bee/format_vector.hpp"
#include "bee/pretty_print.hpp"
#include "bee/sort.hpp"
#include "bee/string_util.hpp"
#include "bee/util.hpp"
#include "bif/array.hpp"
#include "bif/bifer.hpp"
#include "bif/debif.hpp"
#include "bif/string.hpp"
#include "blackbit/color.hpp"
#include "blackbit/game_result.hpp"
#include "blackbit/pieces.hpp"
#include "command/command_builder.hpp"
#include "ml/datapoint.hpp"
#include "ml/ewma.hpp"
#include "ml/fast_tree.hpp"
#include "ml/gut.hpp"
#include "yasf/cof.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <concepts>
#include <memory>
#include <optional>
#include <random>
#include <utility>

using bee::format;
using bee::print_line;
using bee::Span;
using bee::Time;
using std::function;
using std::make_shared;
using std::optional;
using std::shared_ptr;
using std::string;
using std::vector;

using ul = std::unique_lock<std::mutex>;

namespace blackbit {

namespace {

namespace C {

constexpr int batch_size = 10000;
constexpr int batch_per_epoch = 50;
constexpr int sample_pool_size = 1000000;
constexpr int min_samples_to_start = 100000;
constexpr double residual_cap = 4.0;
constexpr double target_lambda = 0.00001;

const ml::GutConfig gut_config = {
  .num_features = FeatureProvider::num_features(),
  .max_tree_nodes = 513,
  .max_tree_height = 32,
  .max_num_trees = 16 * 2,
  .min_samples_to_split = 500000 / 4,
  .learning_rate = 1.0,
  .lr_decay = 0.7,
  .ew_lambda = 0.999,
  .update_threshold = true,
  .loss_function = ml::LossFunction::L2,
};

} // namespace C

struct Work {
  string fen;
};

struct WorkResult {
  vector<float> features;
  Score label;
};

void run_worker(
  int depth,
  shared_ptr<bee::Queue<Work>> work_queue,
  shared_ptr<bee::Queue<bee::OrError<WorkResult>>> result_queue,
  function<EvalParameters()> training_params_factory,
  function<EvalParameters()> target_params_factory)
{
  auto experiment = Experiment::base();

  Board board;
  Board board2;

  auto engine = EngineInProcess::create(
    experiment, EvalParameters::default_params(), nullptr, 1 << 16, true);

  auto run_search =
    [&engine, depth](
      const Board& board,
      EvalParameters&& eval_params) -> bee::OrError<SearchResultInfo::ptr> {
    engine->set_eval_params(std::move(eval_params));
    auto timeout = Span::of_seconds(0.1);
    return engine->find_best_move(board, depth, timeout, nullptr);
  };

  FeatureProvider::vector_type out_features;
  auto make_features = [&out_features, &experiment](const Board& board) {
    out_features.clear();
    auto scratch = Rules::make_scratch(board);
    auto features = Evaluator::features(board, scratch, experiment);
    FeatureProvider::make_features(features, board, out_features);
    return out_features.to_vector();
  };

  auto training_params = training_params_factory();
  auto target_params = target_params_factory();

  int count = 0;

  auto run_work = [&](const Work& work) -> bee::OrError<bee::Unit> {
    board.set_fen(work.fen);
    while (true) {
      ++count;
      if (count >= 32) {
        training_params = training_params_factory();
        target_params = target_params_factory();
        count = 0;
      }

      if (Rules::is_game_over_slow(board)) { break; }

      bail(search_result1, run_search(board, bee::copy(training_params)));
      if (search_result1->eval.is_mate()) {
        // Inevitable mate
        break;
      }

      board2 = board;
      for (const auto& m : search_result1->pv) { board2.move(m); }

      if (Rules::is_game_over_slow(board2)) { break; }

      bail(search_result2, run_search(board2, bee::copy(target_params)));

      result_queue->push(WorkResult{
        .features = make_features(board2),
        .label = search_result2->eval.neg_if(board2.turn == Color::Black),
      });

      board.move(search_result1->best_move);
    }
    return bee::ok();
  };

  for (auto work : *work_queue) {
    auto r = run_work(work);
    if (r.is_error()) { print_line("Worker error: $", r.error()); }
  }
}

bee::OrError<bif::Array<bif::String>> load_fens(const string& filename)
{
  print_line("Loading fens...");
  auto start = Time::monotonic();
  bail(
    content,
    bee::FileReader::read_file_bytes(bee::FilePath::of_string(filename)));
  auto bifer = make_shared<bif::Bifer>(std::move(content));
  auto ret = bif::debif<bif::Array<bif::String>>(bifer);
  print_line(
    "Loading $ positions. Took $", ret.size(), Time::monotonic() - start);
  return ret;
}

struct Trainer {
 public:
  virtual ~Trainer() {}

  virtual double run_step(const vector<ml::DataPoint>& batch) = 0;

  virtual bee::OrError<bee::Unit> save_model(const string& filename) = 0;

  virtual bee::OrError<bee::Unit> init_models(
    const optional<string>& load_model_filename) = 0;

  virtual EvalParameters make_training_params() = 0;

  virtual EvalParameters make_target_params() = 0;

  virtual EvalParameters make_null_params() = 0;

  virtual string info() const = 0;

  virtual string long_info() const = 0;
};

template <class T> struct TSValue {
 public:
  TSValue() {}

  template <class U> TSValue(U&& v) : _v(std::forward<U>(v)) {}

  TSValue(TSValue&&) = delete;
  TSValue& operator=(TSValue&&) = delete;

  T load() const
  {
    ul lk(_mutex);
    return _v;
  }

  template <class U> void store(U&& v)
  {
    ul lk(_mutex);
    _v = std::forward<U>(v);
  }

  template <class U> TSValue operator=(U&& v)
  {
    store(std::forward<U>(v));
    return *this;
  }

  operator T() const { return load(); }

 private:
  mutable std::mutex _mutex;
  T _v;
};

struct TreeTrainer : public Trainer,
                     public std::enable_shared_from_this<TreeTrainer> {
 public:
  TreeTrainer()
  {
    _training_gut = ml::Gut::create(C::gut_config);
    _target_gut = ml::Gut::create(C::gut_config);
    update_fast_trees();
  }

  ~TreeTrainer() {}

  void update_fast_trees()
  {
    _training_fast_trees =
      make_shared<ml::FastTree>(_training_gut->fast_trees());
    _target_fast_trees = make_shared<ml::FastTree>(_target_gut->fast_trees());
  }

  virtual double run_step(const std::vector<ml::DataPoint>& batch) override
  {
    auto loss = _training_gut->run_step(batch);
    _target_gut->update_from(*_training_gut, C::target_lambda);
    update_fast_trees();
    return loss;
  }

  using cof_fmt = std::pair<ml::Gut::ptr, ml::Gut::ptr>;

  virtual bee::OrError<bee::Unit> save_model(const string& filename) override
  {
    return yasf::Cof::serialize_file(
      filename, cof_fmt(_training_gut, _target_gut));
  }

  virtual bee::OrError<bee::Unit> init_models(
    const optional<string>& load_model_filename) override
  {
    if (load_model_filename.has_value()) {
      bail(
        loaded,
        yasf::Cof::deserialize_file<cof_fmt>(
          *load_model_filename, _training_gut->config()));
      _training_gut = std::move(loaded.first);
      _target_gut = std::move(loaded.second);
      update_fast_trees();
    }
    return bee::ok();
  }

  virtual EvalParameters make_training_params() override
  {
    return _make_custom_params(_training_fast_trees, 1.0);
  }

  virtual EvalParameters make_target_params() override
  {
    return _make_custom_params(_target_fast_trees, 1.0);
  }

  virtual EvalParameters make_null_params() override
  {
    return _make_custom_params(_training_fast_trees, 0.0);
  }

  virtual string info() const override
  {
    vector<int> target_sizes = _target_fast_trees.load()->sizes();
    vector<int> training_sizes = _training_fast_trees.load()->sizes();
    return format(
      "training trees: $ [$], target trees: $ [$]",
      training_sizes.size(),
      training_sizes,
      target_sizes.size(),
      target_sizes);
  }

  virtual string long_info() const override
  {
    auto feature_names = FeatureProvider::feature_names();

    auto feature_info =
      [&](const string& name, const vector<int>& freqs) -> string {
      vector<std::pair<int, string>> freq_and_name;
      assert(feature_names.size() >= freqs.size());
      for (int i = 0; i < std::ssize(feature_names); i++) {
        auto& name = feature_names.at(i);
        int f = i < std::ssize(freqs) ? freqs.at(i) : 0;
        freq_and_name.emplace_back(f, name);
      }
      sort(freq_and_name.rbegin(), freq_and_name.rend());
      string lines;
      lines += "===========================================================\n";
      lines += format("$ features:\n", name);
      for (auto& [freq, name] : freq_and_name) {
        lines += format("$: $\n", name, freq);
      }
      return lines;
    };

    string output;
    auto target_frequency = _target_fast_trees.load()->feature_frequency();
    output += feature_info("target", target_frequency);

    auto training_frequency = _training_fast_trees.load()->feature_frequency();
    output += feature_info("training", training_frequency);

    return output;
  }

 private:
  EvalParameters _make_custom_params(
    const shared_ptr<ml::FastTree>& trees, double mult)
  {
    auto params = EvalParameters::default_params();

    params.custom_eval = [mult, trees, f = FeatureProvider::vector_type()](
                           const Features& features,
                           const Board& board) mutable -> Score {
      FeatureProvider::make_features(features, board, f);
      return Score::of_pawns(trees->eval(f) * mult + f[0])
        .neg_if(board.turn == Color::Black);
    };
    return params;
  }

  ml::Gut::ptr _training_gut;
  ml::Gut::ptr _target_gut;

  TSValue<shared_ptr<ml::FastTree>> _training_fast_trees;
  TSValue<shared_ptr<ml::FastTree>> _target_fast_trees;

  std::mt19937 _rng;
};

shared_ptr<Trainer> create_trainer() { return make_shared<TreeTrainer>(); }

struct SamplePool {
 public:
  SamplePool(int max_samples, int seed) : _max_samples(max_samples), _rng(seed)
  {
    _samples.reserve(max_samples);
    _sampling_indices.reserve(max_samples);
    for (int i = 0; i < max_samples; i++) { _sampling_indices.push_back(i); }
  }

  void add_samples(vector<ml::DataPoint>&& samples)
  {
    ul lk(_mutex);
    _num_samples_added += std::ssize(samples);
    for (auto& dp : samples) {
      if (std::ssize(_samples) < _max_samples) {
        _samples.emplace_back(std::move(dp));
      } else {
        if (_next_add_idx >= std::ssize(_samples)) { _next_add_idx = 0; }
        _samples[_next_add_idx] = std::move(dp);
      }
    }
  }

  vector<ml::DataPoint> take_batch(int batch_size)
  {
    ul lk(_mutex);
    assert(!_samples.empty());
    vector<ml::DataPoint> out;
    out.reserve(batch_size);
    while (std::ssize(out) < batch_size) {
      if (_next_sampling_idx >= std::ssize(_sampling_indices)) {
        _next_sampling_idx = 0;
        std::shuffle(_sampling_indices.begin(), _sampling_indices.end(), _rng);
        _num_shuffles++;
      }
      int idx = _sampling_indices[_next_sampling_idx++];
      if (idx >= std::ssize(_samples)) { continue; }
      out.push_back(_samples[idx]);
    }
    return out;
  }

  int64_t num_samples_added()
  {
    ul lk(_mutex);
    return _num_samples_added;
  }

  int64_t size()
  {
    ul lk(_mutex);
    return std::ssize(_samples);
  }

  int num_shuffles()
  {
    ul lk(_mutex);
    return _num_shuffles;
  }

 private:
  int _replace_idx()
  {
    std::uniform_int_distribution<int> dist(0, _samples.size() - 1);
    return dist(_rng);
  }

  std::mutex _mutex;

  const int _max_samples;
  vector<ml::DataPoint> _samples;
  int _next_add_idx = 0;

  vector<int> _sampling_indices;
  int _next_sampling_idx = 0;

  int _num_shuffles = 0;

  int64_t _num_samples_added = 0;

  std::mt19937 _rng;
};

bee::OrError<bee::Unit> training_main(
  const string& positions_file,
  const int training_depth,
  const int num_workers,
  const string& save_model_filename,
  const optional<string>& load_model_filename,
  const optional<double>& max_training_hours)
{
  randomize_seed();

  std::mt19937 rng((std::random_device()()));

  int num_features = FeatureProvider::num_features();

  optional<Span> max_training_time;
  if (max_training_hours.has_value()) {
    max_training_time = Span::of_hours(*max_training_hours);
    print_line("Will train for at most $", *max_training_time);
  }

  auto feature_names = FeatureProvider::feature_names();

  {
    print_line("Num features: $", num_features);

    int idx = 0;
    print_line("feature names");
    for (const auto& n : feature_names) { print_line("$: $", idx++, n); }
  }

  auto trainer = create_trainer();
  bail_unit(trainer->init_models(load_model_filename));

  auto training_params_factory = [=]() {
    return trainer->make_training_params();
  };
  auto target_params_factory = [=]() { return trainer->make_target_params(); };

  auto work_queue = make_shared<bee::Queue<Work>>(16);
  auto result_queue = make_shared<bee::Queue<bee::OrError<WorkResult>>>(16);

  auto sample_pool = make_shared<SamplePool>(C::sample_pool_size, rng());

  vector<std::thread> workers;
  for (int i = 0; i < num_workers; i++) {
    workers.emplace_back(
      run_worker,
      training_depth,
      work_queue,
      result_queue,
      training_params_factory,
      target_params_factory);
  }

  auto exiting = make_shared<std::atomic<bool>>(false);

  bail(fens, load_fens(positions_file));
  auto producer_seed = rng();
  std::thread producer(
    [fens = std::move(fens), exiting, work_queue, producer_seed]() {
      std::mt19937 rng(producer_seed);
      vector<int> fen_ids;
      fen_ids.reserve(std::ssize(fens));
      for (int i = 0; i < std::ssize(fens); i++) { fen_ids.push_back(i); }
      while (true) {
        std::shuffle(fen_ids.begin(), fen_ids.end(), rng);
        for (int id : fen_ids) {
          if (exiting->load() || !work_queue->push(Work{.fen = fens.at(id)})) {
            return;
          }
        }
      }
    });

  std::thread consumer([=]() {
    vector<ml::DataPoint> batch;
    while (true) {
      auto result = result_queue->pop();
      if (!result.has_value()) { break; }
      if (result->is_error()) { continue; }
      auto& rs = result.value().value();

      double label = std::clamp(
        rs.label.to_pawns() - rs.features[0],
        -C::residual_cap,
        C::residual_cap);

      batch.push_back({
        .features = std::move(rs.features),
        .label = label,
      });
      if (batch.size() >= 128) {
        sample_pool->add_samples(std::move(batch));
        batch.clear();
      }
    }
  });

  ml::Ewma loss_ewma(0.99);
  ml::Ewma samples_per_second(0.9);

  auto training_start = Time::monotonic();

  while (true) {
    int num_samples = sample_pool->num_samples_added();
    if (num_samples >= C::min_samples_to_start) { break; }
    print_line("num samples: $", num_samples);
    Span::of_seconds(1.0).sleep();
  }

  int64_t last_num_samples = sample_pool->num_samples_added();

  int epoch = 0;
  while (true) {
    print_line("---------------------------------------------------------");
    epoch++;

    auto batch_start = Time::monotonic();

    double loss_sum = 0;

    for (int i = 0; i < C::batch_per_epoch; i++) {
      auto batch = sample_pool->take_batch(C::batch_size);

      auto loss = trainer->run_step(batch);
      loss_sum += loss;
      loss_ewma.add(loss);
    }

    bail_unit(trainer->save_model(save_model_filename));

    auto batch_end = Time::monotonic();

    auto total_time = batch_end - training_start;
    auto epoch_time = batch_end - batch_start;

    auto num_samples_added = sample_pool->num_samples_added();
    auto samples_in_epoch = num_samples_added - last_num_samples;
    samples_per_second.add(samples_in_epoch / epoch_time.to_float_seconds());
    last_num_samples = num_samples_added;

    print_line("epoch: $", epoch);
    print_line(
      "loss (ewma): $ ($)", loss_sum / C::batch_per_epoch, loss_ewma.avg());
    print_line("model info: $", trainer->info());
    print_line("num samples: $", sample_pool->size());
    print_line("num all samples: $", sample_pool->num_samples_added());
    print_line("samples/s: $", samples_per_second.avg());
    print_line("num shuffles: $", sample_pool->num_shuffles());
    print_line("epoch time: $", epoch_time);
    print_line("total time: $/$", total_time, max_training_time);

    if (max_training_time.has_value() && total_time > *max_training_time) {
      break;
    }
  }

  exiting->store(true);
  work_queue->close();
  result_queue->close();

  producer.join();
  consumer.join();
  for (auto& w : workers) { w.join(); }

  return bee::unit;
}

bee::OrError<bee::Unit> evaluate_main(
  const string& positions_file,
  const double seconds_per_move,
  const int num_workers,
  const int num_rounds,
  const int max_depth,
  const string& result_filename,
  const string& load_model_filename,
  bool use_null_model_for_baseline)
{
  auto trainer = create_trainer();

  bail_unit(trainer->init_models(load_model_filename));
  print_line(trainer->info());
  print_line(trainer->long_info());

  auto default_model = EvalParameters::default_params();

  auto create_params_creator = [](const EvalParameters& eval_params) {
    return [eval_params]() {
      return EngineParams{
        .experiment = Experiment::base(),
        .eval_params = eval_params,
      };
    };
  };

  auto create_baseline_params = [=]() {
    if (use_null_model_for_baseline) {
      return create_params_creator(trainer->make_null_params());
    } else {
      return create_params_creator(EvalParameters::default_params());
    }
  };

  auto create_base_params = create_baseline_params();
  auto create_test_params =
    create_params_creator(trainer->make_training_params());

  return CompareEngines::compare(
    positions_file,
    seconds_per_move,
    num_rounds,
    num_workers,
    1,
    max_depth,
    result_filename,
    create_base_params,
    create_test_params);
}

bee::OrError<bee::Unit> benchmark_main(const string& load_model_filename)
{
  auto trainer = create_trainer();

  bail_unit(trainer->init_models(load_model_filename));
  print_line(trainer->info());
  print_line(trainer->long_info());

  auto run_one = [](EvalParameters&& eval_params) -> bee::OrError<bee::Unit> {
    uint64_t nodes = 0;
    int depth_sum = 0;

    auto create_engine = [](EvalParameters&& eval_params) {
      auto experiment = Experiment::base();
      return EngineInProcess::create(
        experiment, std::move(eval_params), nullptr, 1 << 30, true);
    };

    auto engine = create_engine(std::move(eval_params));

    Board board;
    board.set_initial();

    const int repeat = 100;

    for (int i = 0; i < repeat; i++) {
      bail(
        result,
        engine->find_best_move(board, 50, Span::of_seconds(1), nullptr));

      nodes += result->nodes;
      depth_sum += result->depth;
    }

    print_line("nodes: $", double(nodes) / repeat);
    print_line("depth: $", double(depth_sum) / repeat);
    print_line("------------------------------");
    return bee::ok();
  };

  {
    print_line("Null params:");
    bail_unit(run_one(trainer->make_null_params()));
  }

  {
    print_line("Training params:");
    bail_unit(run_one(trainer->make_training_params()));
  }

  {
    print_line("Target params:");
    bail_unit(run_one(trainer->make_target_params()));
  }

  {
    print_line("Default:");
    bail_unit(run_one(EvalParameters::default_params()));
  }

  return bee::ok();
}

using command::Cmd;
using command::CommandBuilder;

const string default_model_name = "model-latest.cof";

Cmd training_command()
{
  using namespace command::flags;
  auto builder = CommandBuilder("Train the bot");
  auto positions_file = builder.required("--positions-file", string_flag);
  auto training_depth =
    builder.optional_with_default("--training-depth", int_flag, 6);
  auto num_workers = builder.optional_with_default("--workers", int_flag, 8);
  auto save_model_filename = builder.optional_with_default(
    "--save-model-file", string_flag, default_model_name);
  auto load_model_filename = builder.optional("--load-model-file", string_flag);
  auto max_training_hours =
    builder.optional("--max-training-hours", float_flag);
  return builder.run([=] {
    return training_main(
      *positions_file,
      *training_depth,
      *num_workers,
      *save_model_filename,
      *load_model_filename,
      *max_training_hours);
  });
}

Cmd evaluate_command()
{
  using namespace command::flags;
  auto builder = CommandBuilder("Evaluate a trained model");
  auto positions_file = builder.required("--positions-file", string_flag);
  auto seconds_per_move =
    builder.optional_with_default("--seconds-per-move", float_flag, 2.0);
  auto num_workers = builder.optional_with_default("--workers", int_flag, 14);
  auto num_rounds =
    builder.optional_with_default("--num-rounds", int_flag, 10000);
  auto result_filename = builder.required("--result-file", string_flag);
  auto load_model_filename = builder.optional_with_default(
    "--load-model-file", string_flag, default_model_name);
  auto use_null_model_for_baseline = builder.no_arg("--null-model-baseline");
  auto max_depth = builder.optional_with_default("--max-depth", int_flag, 50);
  return builder.run([=] {
    return evaluate_main(
      *positions_file,
      *seconds_per_move,
      *num_workers,
      *num_rounds,
      *max_depth,
      *result_filename,
      *load_model_filename,
      *use_null_model_for_baseline);
  });
}

Cmd benchmark_command()
{
  using namespace command::flags;
  auto builder = CommandBuilder("Benchmark model");
  auto load_model_filename = builder.optional_with_default(
    "--load-model-file", string_flag, default_model_name);
  return builder.run([=] { return benchmark_main(*load_model_filename); });
}

} // namespace

command::Cmd Training::command()
{
  return command::GroupBuilder("Training")
    .cmd("train", training_command())
    .cmd("evaluate", evaluate_command())
    .cmd("benchmark", benchmark_command())
    .build();
}

} // namespace blackbit
