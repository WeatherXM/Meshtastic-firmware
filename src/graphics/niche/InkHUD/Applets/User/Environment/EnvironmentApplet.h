#ifdef MESHTASTIC_INCLUDE_INKHUD

#pragma once

#include "configuration.h"
#include "graphics/niche/InkHUD/Applet.h"
#include "mesh/SinglePortModule.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"

namespace NicheGraphics::InkHUD
{

class EnvironmentApplet : public Applet, public SinglePortModule
{
  public:
    EnvironmentApplet();

    void onRender(bool full) override;
    void onActivate() override;
    void onDeactivate() override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  protected:
    bool hasData = false;
    meshtastic_EnvironmentMetrics envData = meshtastic_EnvironmentMetrics_init_zero;
    NodeNum senderNodeNum = 0;
    std::string senderName;
    uint32_t lastRxTime = 0;
};

} // namespace NicheGraphics::InkHUD

#endif
