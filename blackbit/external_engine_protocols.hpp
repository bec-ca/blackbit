#pragma once

#include "external_engine.hpp"

namespace blackbit {

EngineProtocol::ptr create_xboard_client_protocol();

EngineProtocol::ptr create_uci_client_protocol();

} // namespace blackbit
