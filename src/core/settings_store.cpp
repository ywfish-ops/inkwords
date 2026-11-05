#include "settings_store.h"
#include <Preferences.h>
#include <string.h>

static const char* NVS_NAMESPACE = "spell";

void settings_load(UserSettings& settings) {
    memset(&settings, 0, sizeof(settings));
    strncpy(settings.cat_filter, "all", sizeof(settings.cat_filter) - 1);

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    uint8_t vol = prefs.getUChar("vol", 100);
    uint8_t dir = prefs.getUChar("dir", DIR_EN_TO_ZH);
    settings.auto_speak = prefs.getBool("auto", false);
    settings.new_per_day = prefs.getUChar("newpd", 10);
    prefs.end();

    settings.volume = vol > 100 ? 100 : vol;
    settings.dir = (dir == DIR_ZH_TO_EN) ? DIR_ZH_TO_EN : DIR_EN_TO_ZH;
}

void settings_save(const UserSettings& settings) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar("vol", settings.volume);
    prefs.putUChar("dir", (uint8_t)settings.dir);
    prefs.putBool("auto", settings.auto_speak);
    prefs.putUChar("newpd", settings.new_per_day);
    prefs.end();
}
