#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
import glob
import os

Import("env")

# Ensure meshtastic-device-ui has GFX_DRIVER_INC support in DisplayDriverFactory.cpp
libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
pattern = os.path.join(
    libdeps_dir,
    "*",
    "meshtastic-device-ui",
    "source",
    "graphics",
    "driver",
    "DisplayDriverFactory.cpp",
)

for path in glob.glob(pattern):
    if os.path.exists(path):
        with open(path, "r") as f:
            content = f.read()
        if "GFX_DRIVER_INC" not in content:
            target = "#ifndef ARCH_PORTDUINO\n"
            replacement = (
                target + "#ifdef GFX_DRIVER_INC\n#include GFX_DRIVER_INC\n#endif\n"
            )
            if target in content:
                content = content.replace(target, replacement, 1)
                with open(path, "w") as f:
                    f.write(content)
                print(f"Patched {path} with GFX_DRIVER_INC support")

# Ensure meshtastic-device-ui grants WeatherXMTFTView access to ui_set_active
tft_header_pattern = os.path.join(
    libdeps_dir,
    "*",
    "meshtastic-device-ui",
    "include",
    "graphics",
    "view",
    "TFT",
    "TFTView_320x240.h",
)

for path in glob.glob(tft_header_pattern):
    if os.path.exists(path):
        with open(path, "r") as f:
            content = f.read()
        if "WeatherXMTFTView" not in content:
            target = "friend class ViewFactory;\n"
            replacement = (
                "friend class ViewFactory;\n    friend class WeatherXMTFTView;\n"
            )
            if target in content:
                content = content.replace(target, replacement, 1)
                with open(path, "w") as f:
                    f.write(content)
                print(f"Patched {path} with WeatherXMTFTView friend class")

# Ensure TFTView_320x240.cpp does not hang on 'Rebooting...' after initial setup or username change
tft_source_pattern = os.path.join(
    libdeps_dir,
    "*",
    "meshtastic-device-ui",
    "source",
    "graphics",
    "TFT",
    "TFTView_320x240.cpp",
)

for path in glob.glob(tft_source_pattern):
    if os.path.exists(path):
        with open(path, "r") as f:
            content = f.read()
        modified = False

        # 1. In case eSetup, update channel names and frequency when region changes
        setup_cfg_target = """                lora.channel_num = (defaultSlot <= numChannels ? defaultSlot : 1);
                THIS->controller->sendConfig(meshtastic_Config_LoRaConfig{lora}, THIS->ownNode);
            }"""
        setup_cfg_replacement = """                lora.channel_num = (defaultSlot <= numChannels ? defaultSlot : 1);
                for (int i = 0; i < c_max_channels; i++) {
                    if (THIS->db.channel[i].has_settings && THIS->db.channel[i].role != meshtastic_Channel_Role_DISABLED) {
                        THIS->setChannelName(THIS->db.channel[i]);
                    }
                }
                THIS->controller->sendConfig(meshtastic_Config_LoRaConfig{lora}, THIS->ownNode);
                THIS->showLoRaFrequency(lora);
            }"""
        if setup_cfg_target in content:
            content = content.replace(setup_cfg_target, setup_cfg_replacement, 1)
            modified = True

        # 2. In case eSetup, switch back to home screen without bogus notifyReboot
        setup_reboot_target = """            THIS->notifyReboot(true);

            lv_obj_add_flag(objects.initial_setup_panel, LV_OBJ_FLAG_HIDDEN);
            lv_group_focus_obj(objects.home_button);
            break;"""
        setup_reboot_replacement = """            THIS->ui_set_active(objects.home_button, objects.home_panel, objects.top_panel);
            lv_group_focus_obj(objects.home_button);
            break;"""
        if setup_reboot_target in content:
            content = content.replace(setup_reboot_target, setup_reboot_replacement, 1)
            modified = True

        # 3. In case eUsername, remove bogus notifyReboot
        username_reboot_target = """                THIS->controller->sendConfig(user, THIS->ownNode);
                THIS->notifyReboot(true);
            }
            lv_obj_add_flag(objects.settings_username_panel, LV_OBJ_FLAG_HIDDEN);"""
        username_reboot_replacement = """                THIS->controller->sendConfig(user, THIS->ownNode);
            }
            lv_obj_add_flag(objects.settings_username_panel, LV_OBJ_FLAG_HIDDEN);"""
        if username_reboot_target in content:
            content = content.replace(
                username_reboot_target, username_reboot_replacement, 1
            )
            modified = True

        if modified:
            with open(path, "w") as f:
                f.write(content)
            print(f"Patched {path} to prevent reboot hang on setup/username change")
