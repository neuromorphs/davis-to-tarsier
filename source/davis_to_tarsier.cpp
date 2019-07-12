#define LIBCAER_FRAMECPP_OPENCV_INSTALLED 0
#include "../third_party/chameleon/source/background_cleaner.hpp"
#include "../third_party/chameleon/source/dvs_display.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/tarsier/source/mirror_x.hpp"
#include "../third_party/tarsier/source/select_rectangle.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <atomic>
#include <csignal>
#include <iostream>
#include <libcaercpp/devices/davis.hpp>
#include <thread>

using namespace std;

static atomic_bool globalShutdown(false);

static void globalShutdownSignalHandler(int signal) {
  // Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for
  // global shutdown.
  if (signal == SIGTERM || signal == SIGINT) {
    globalShutdown.store(true);
  }
}

static void usbShutdownHandler(void *ptr) {
  (void)(ptr); // UNUSED.

  globalShutdown.store(true);
}

int main(int argc, char *argv[]) {

// Install signal handler for global shutdown.
#if defined(_WIN32)
  if (signal(SIGTERM, &globalShutdownSignalHandler) == SIG_ERR) {
    libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                      "Failed to set signal handler for SIGTERM. Error: %d.",
                      errno);
    return (EXIT_FAILURE);
  }

  if (signal(SIGINT, &globalShutdownSignalHandler) == SIG_ERR) {
    libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                      "Failed to set signal handler for SIGINT. Error: %d.",
                      errno);
    return (EXIT_FAILURE);
  }
#else
  struct sigaction shutdownAction;

  shutdownAction.sa_handler = &globalShutdownSignalHandler;
  shutdownAction.sa_flags = 0;
  sigemptyset(&shutdownAction.sa_mask);
  sigaddset(&shutdownAction.sa_mask, SIGTERM);
  sigaddset(&shutdownAction.sa_mask, SIGINT);

  if (sigaction(SIGTERM, &shutdownAction, NULL) == -1) {
    libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                      "Failed to set signal handler for SIGTERM. Error: %d.",
                      errno);
    return (EXIT_FAILURE);
  }

  if (sigaction(SIGINT, &shutdownAction, NULL) == -1) {
    libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                      "Failed to set signal handler for SIGINT. Error: %d.",
                      errno);
    return (EXIT_FAILURE);
  }
#endif

  // Open a DAVIS, give it a device ID of 1, and don't care about USB bus or SN
  // restrictions.
  libcaer::devices::davis davisHandle = libcaer::devices::davis(1);

  // Let's take a look at the information we have on the device.
  struct caer_davis_info davis_info = davisHandle.infoGet();

  printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n",
         davis_info.deviceString, davis_info.deviceID,
         davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
         davis_info.logicVersion);

  // Send the default configuration before using the device.
  // No configuration is sent automatically!
  davisHandle.sendDefaultConfig();

  // Tweak some biases, to increase bandwidth in this case.
  struct caer_bias_coarsefine coarseFineBias;

  coarseFineBias.coarseValue = 2;
  coarseFineBias.fineValue = 116;
  coarseFineBias.enabled = true;
  coarseFineBias.sexN = false;
  coarseFineBias.typeNormal = true;
  coarseFineBias.currentLevelNormal = true;

  davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP,
                        caerBiasCoarseFineGenerate(coarseFineBias));

  coarseFineBias.coarseValue = 1;
  coarseFineBias.fineValue = 33;
  coarseFineBias.enabled = true;
  coarseFineBias.sexN = false;
  coarseFineBias.typeNormal = true;
  coarseFineBias.currentLevelNormal = true;

  davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP,
                        caerBiasCoarseFineGenerate(coarseFineBias));

  // Let's verify they really changed!
  uint32_t prBias =
      davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP);
  uint32_t prsfBias =
      davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP);

  printf("New bias values --- PR-coarse: %d, PR-fine: %d, PRSF-coarse: %d, "
         "PRSF-fine: %d.\n",
         caerBiasCoarseFineParse(prBias).coarseValue,
         caerBiasCoarseFineParse(prBias).fineValue,
         caerBiasCoarseFineParse(prsfBias).coarseValue,
         caerBiasCoarseFineParse(prsfBias).fineValue);

  // Now let's get start getting some data from the device. We just loop in
  // blocking mode,
  // no notification needed regarding new events. The shutdown notification, for
  // example if
  // the device is disconnected, should be listened to.
  davisHandle.dataStart(nullptr, nullptr, nullptr, &usbShutdownHandler,
                        nullptr);

  // Let's turn on blocking data-get mode to avoid wasting resources.
  davisHandle.configSet(CAER_HOST_CONFIG_DATAEXCHANGE,
                        CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);

  QGuiApplication app(argc, argv);

  // register Chameleon types
  qmlRegisterType<chameleon::background_cleaner>("Chameleon", 1, 0,
                                                 "BackgroundCleaner");
  qmlRegisterType<chameleon::dvs_display>("Chameleon", 1, 0, "DvsDisplay");

  // pass the header properties to qml
  QQmlApplicationEngine application_engine;
  application_engine.rootContext()->setContextProperty("header_width",
                                                       davis_info.dvsSizeX);
  application_engine.rootContext()->setContextProperty("header_height",
                                                       davis_info.dvsSizeY);

  // load the view and setup the window properties for OpenGL rendering
  application_engine.loadData(
#include "davis_reader.qml"
      );
  auto window =
      qobject_cast<QQuickWindow *>(application_engine.rootObjects().first());
  {
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    window->setFormat(format);
  }

  // retrieve pointers to the displays generated by qml
  auto dvs_display = window->findChild<chameleon::dvs_display *>("dvs_display");

  auto _loop = std::thread([&]() {

    while (!globalShutdown.load(memory_order_relaxed)) {
      std::unique_ptr<libcaer::events::EventPacketContainer> packetContainer =
          davisHandle.dataGet();
      if (packetContainer == nullptr) {
        continue; // Skip if nothing there.
      }

      for (auto &packet : *packetContainer) {
        if (packet == nullptr) {
          continue; // Skip if nothing there.
        }

        if (packet->getEventType() == POLARITY_EVENT) {
          std::shared_ptr<const libcaer::events::PolarityEventPacket> polarity =
              std::static_pointer_cast<libcaer::events::PolarityEventPacket>(
                  packet);

          auto select_rectangle =
              tarsier::make_select_rectangle<sepia::dvs_event>(
                  0, 0, 240, 180,
                  [&](sepia::dvs_event event) { dvs_display->push(event); });

          for (auto event : *polarity) {
            auto sepia_event = sepia::dvs_event{
                static_cast<uint64_t>(event.getTimestamp()), event.getX(),
                static_cast<uint16_t>(davis_info.dvsSizeY - event.getY()),
                static_cast<bool>(event.getPolarity())};
            select_rectangle(sepia_event);
          }
        }

        if (packet->getEventType() == FRAME_EVENT) {
						//skip
        }
      }
    }

    davisHandle.dataStop();

    printf("Shutdown successful.\n");
  });

  return app.exec();
}
