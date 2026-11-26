#pragma once
#include "crow_all.h"
#include "services/app_service.h"

namespace ig::controllers {
void registerRoutes(crow::SimpleApp& app, AppContext& context,
                    AppService& service);
}
