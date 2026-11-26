#include <iostream>
#include <string>

#include "crow_all.h"
#include "controllers/routes.h"
#include "core/config.h"
#include "services/app_service.h"

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  ig::AppConfig config = ig::AppConfig::fromEnvironment();

  ig::AppContext context;
  std::string error;
  if (!context.initialize(config, &error)) {
    std::cerr << "Startup failed: " << error << '\n';
    return 1;
  }
  std::cout << "Connected to MySQL " << config.mysqlHost << ":" << config.mysqlPort
            << " and rebuilt DSA indexes\n";

  ig::AppService service(context);
  crow::SimpleApp app;
  ig::controllers::registerRoutes(app, context, service);

  std::cout << "Instagram clone backend listening on " << config.host << ":"
            << config.port << '\n';
  app.port(config.port).bindaddr(config.host).multithreaded().run();
  return 0;
}
