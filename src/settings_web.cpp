#include "settings_web.h"

#include <Arduino.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{
const char SETTINGS_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MoonBoard Bridge</title><style>
:root{font-family:system-ui,sans-serif;color:#17202a;background:#eef2f5}
body{max-width:850px;margin:auto;padding:16px}h1{margin-bottom:4px}
section{background:white;border-radius:12px;padding:18px;margin:16px 0;box-shadow:0 2px 10px #0001}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
label{display:flex;flex-direction:column;gap:5px;font-weight:600}input,textarea,button{font:inherit;padding:9px;border:1px solid #b8c2cc;border-radius:7px}
input[type=checkbox]{width:auto}label.check{flex-direction:row;align-items:center;margin-top:26px}
button{background:#175cd3;color:white;border:0;cursor:pointer}button.secondary{background:#536273}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}.status{min-height:24px;font-weight:600}.error{color:#b42318}.ok{color:#027a48}
textarea{min-height:105px;font-family:monospace;font-weight:400}.hint{color:#536273;font-size:.9rem}
</style></head><body><h1>MoonBoard Bridge</h1><div id="summary"></div>
<p><a href="/logs">Live-Log öffnen</a></p>
<section><h2>Einstellungen</h2><form id="settings"><div class="grid">
<label>WLED-Adresse<input id="host" required maxlength="63"></label>
<label>Gesamte physische LEDs<input id="physical" type="number" min="1" max="2048" required></label>
<label>WLED-Segment-ID<input id="segment" type="number" min="0" max="255" required></label>
<label>Routenhelligkeit (%)<input id="boulder" type="number" min="0" max="100" required></label>
<label>Zusatzlicht oberhalb (%)<input id="above" type="number" min="0" max="100" required></label>
<label>Abschaltung (Minuten, 0 = nie)<input id="timeout" type="number" min="0" max="65535" required></label>
<label class="check"><input id="kickerEnabled" type="checkbox"> Kicker-LEDs mit Route einschalten</label>
<label>Kicker-Farbe<input id="kickerColor" type="color"></label>
<label style="grid-column:1/-1">Physische Kicker-LED-IDs (CSV)<input id="kickerIds" placeholder="0,3,6,9"></label>
</div><div class="actions"><button type="submit">Einstellungen speichern</button><button type="button" class="secondary" id="off">Alle LEDs aus</button></div></form></section>
<section><h2>LED-Kalibrierung</h2><p class="hint">Jede Test-LED wird nach fünf Sekunden automatisch ausgeschaltet. Das Mapping wird nach jeder Zuweisung dauerhaft gespeichert.</p>
<div class="grid"><label>Logische Position<input id="logical" type="number" min="0"></label>
<label>MoonBoard-Koordinate<input id="coordinate" readonly></label>
<label>Physische WLED-ID<input id="mapped" type="number" min="0" max="2047"></label></div>
<div class="actions"><button type="button" class="secondary" id="previous">Zurück</button><button type="button" class="secondary" id="next">Weiter</button><button type="button" id="testLed">LED testen</button><button type="button" id="assign">Zuordnen und speichern</button></div>
<h3>Mapping importieren/exportieren</h3><textarea id="mapping"></textarea>
<div class="actions"><button type="button" class="secondary" id="export">Aktuelles Mapping anzeigen</button><button type="button" id="import">Mapping prüfen und speichern</button></div></section>
<div id="status" class="status"></div><p class="hint">Die Oberfläche besitzt keine Anmeldung und darf nur in einem vertrauenswürdigen lokalen Netzwerk erreichbar sein.</p>
<script>
let cfg;
const $=id=>document.getElementById(id);
function status(text,error=false){$('status').textContent=text;$('status').className='status '+(error?'error':'ok')}
function colorHex(c){return '#'+[c.r,c.g,c.b].map(v=>v.toString(16).padStart(2,'0')).join('')}
function columnName(column){let s='';for(let n=column+1;n>0;n=Math.floor((n-1)/26))s=String.fromCharCode(65+(n-1)%26)+s;return s}
function coordinate(position){const col=Math.floor(position/cfg.boardRows),p=position%cfg.boardRows,row=col%2?cfg.boardRows-p:p+1;return columnName(col)+row}
function showMapping(){let i=Math.max(0,Math.min(cfg.logicalLedCount-1,Number($('logical').value)||0));$('logical').value=i;$('coordinate').value=coordinate(i);$('mapped').value=cfg.mapping[i]}
async function post(path,data){const response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)});const result=await response.json();if(!response.ok||!result.ok)throw new Error(result.message||'Fehler');return result}
async function load(){const response=await fetch('/api/config');cfg=await response.json();$('summary').textContent=`${cfg.board} · ${cfg.logicalLedCount} MoonBoard-Positionen`;$('host').value=cfg.wledHost;$('physical').value=cfg.physicalLedCount;$('segment').value=cfg.segmentId;$('boulder').value=cfg.boulderBrightnessPercent;$('above').value=cfg.aboveHoldBrightnessPercent;$('timeout').value=cfg.routeTimeoutMinutes;$('kickerEnabled').checked=cfg.kickerLedsEnabled;$('kickerColor').value=colorHex(cfg.kickerColor);$('kickerIds').value=cfg.kickerIds.join(',');$('logical').max=cfg.logicalLedCount-1;showMapping()}
$('settings').onsubmit=async e=>{e.preventDefault();try{await post('/api/settings',{host:$('host').value,physical:$('physical').value,segment:$('segment').value,boulder:$('boulder').value,above:$('above').value,timeout:$('timeout').value,kickerEnabled:$('kickerEnabled').checked?'1':'0',kickerColor:$('kickerColor').value,kickerIds:$('kickerIds').value});await load();status('Einstellungen dauerhaft gespeichert.')}catch(e){status(e.message,true)}};
$('logical').oninput=showMapping;$('previous').onclick=()=>{$('logical').value=Math.max(0,Number($('logical').value)-1);showMapping()};$('next').onclick=()=>{$('logical').value=Math.min(cfg.logicalLedCount-1,Number($('logical').value)+1);showMapping()};
$('testLed').onclick=async()=>{try{await post('/api/test-led',{physical:$('mapped').value});status('Test-LED für fünf Sekunden eingeschaltet.')}catch(e){status(e.message,true)}};
$('assign').onclick=async()=>{try{const logical=Number($('logical').value),physical=Number($('mapped').value);await post('/api/mapping',{logical,physical});cfg.mapping[logical]=physical;status(`${coordinate(logical)} wurde WLED-ID ${physical} zugeordnet.`)}catch(e){status(e.message,true)}};
$('export').onclick=()=>{$('mapping').value=cfg.mapping.join(',');status('Mapping im Textfeld bereitgestellt.')};
$('import').onclick=async()=>{try{await post('/api/mapping-list',{mapping:$('mapping').value});await load();status('Vollständiges Mapping dauerhaft gespeichert.')}catch(e){status(e.message,true)}};
$('off').onclick=async()=>{try{await post('/api/off',{});status('Alle LEDs ausgeschaltet.')}catch(e){status(e.message,true)}};
load().catch(e=>status(e.message,true));
</script></body></html>
)HTML";

const char LOG_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MoonBoard Live-Log</title><style>
:root{font-family:system-ui,sans-serif;color:#e6edf3;background:#0d1117}
body{max-width:1100px;margin:auto;padding:16px}a{color:#58a6ff}h1{margin-bottom:4px}
.toolbar{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin:16px 0}
button{font:inherit;padding:9px 14px;border:0;border-radius:7px;background:#238636;color:white;cursor:pointer}
label{display:flex;gap:6px;align-items:center}.state{font-weight:600;color:#3fb950}.error{color:#f85149}
pre{box-sizing:border-box;min-height:65vh;max-height:75vh;overflow:auto;white-space:pre-wrap;word-break:break-word;background:#010409;border:1px solid #30363d;border-radius:8px;padding:14px;margin:0;color:#c9d1d9}
.hint{color:#8b949e;font-size:.9rem}
</style></head><body><h1>MoonBoard Live-Log</h1>
<p><a href="/">Zurück zu Einstellungen und Kalibrierung</a></p>
<div class="toolbar"><button id="clear">Anzeige leeren</button><button id="pause">Pausieren</button>
<label><input id="scroll" type="checkbox" checked> Automatisch scrollen</label><span id="state" class="state">Verbinde …</span></div>
<pre id="log"></pre><p class="hint">Die letzten 80 Firmwaremeldungen liegen flüchtig im RAM. Ein Neustart löscht den Puffer. Die Seite aktualisiert sich einmal pro Sekunde und besitzt keine Anmeldung.</p>
<script>
let after=0,paused=false,busy=false;
const log=document.getElementById('log'),state=document.getElementById('state'),pause=document.getElementById('pause');
function append(entries){for(const entry of entries){const seconds=(entry.ms/1000).toFixed(3).padStart(10,' ');log.append(document.createTextNode(`[${seconds}s] ${entry.text}\n`))}while(log.childNodes.length>500)log.firstChild.remove();if(document.getElementById('scroll').checked)log.scrollTop=log.scrollHeight}
async function poll(){if(paused||busy)return;busy=true;try{const response=await fetch(`/api/logs?after=${after}`,{cache:'no-store'});const result=await response.json();if(!response.ok)throw new Error(result.message||'HTTP-Fehler');append(result.entries);after=result.next;state.textContent='Verbunden';state.className='state'}catch(error){state.textContent=`Nicht verbunden: ${error.message}`;state.className='state error'}finally{busy=false}}
document.getElementById('clear').onclick=()=>{log.textContent=''};
pause.onclick=()=>{paused=!paused;pause.textContent=paused?'Fortsetzen':'Pausieren';state.textContent=paused?'Pausiert':'Verbinde …';state.className='state';if(!paused)poll()};
setInterval(poll,1000);poll();
</script></body></html>
)HTML";

std::string jsonEscape(const char *value)
{
    std::string result;
    if (value == nullptr)
        return result;
    while (*value != '\0')
    {
        if (*value == '"' || *value == '\\')
            result += '\\';
        result += *value++;
    }
    return result;
}

bool parseColor(const String &text, RgbColor &color)
{
    const char *start = text.c_str();
    if (*start == '#')
        ++start;
    if (std::strlen(start) != 6)
        return false;
    for (size_t index = 0; index < 6; ++index)
    {
        if (!std::isxdigit(static_cast<unsigned char>(start[index])))
            return false;
    }
    char *end = nullptr;
    const unsigned long value = std::strtoul(start, &end, 16);
    if (end == start || *end != '\0')
        return false;
    color = RgbColor(
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF));
    return true;
}
} // namespace

SettingsWebServer::SettingsWebServer()
    : server_(80),
      settings_(nullptr),
      liveLog_(nullptr),
      boardName_(""),
      boardRows_(0)
{
}

void SettingsWebServer::begin(
    RuntimeSettings *settings,
    LiveLogBuffer *liveLog,
    const char *boardName,
    uint8_t boardRows,
    ApplySettingsHandler applySettings,
    TestLedHandler testLed,
    AllOffHandler allOff)
{
    settings_ = settings;
    liveLog_ = liveLog;
    boardName_ = boardName;
    boardRows_ = boardRows;
    applySettings_ = applySettings;
    testLed_ = testLed;
    allOff_ = allOff;

    server_.on("/", HTTP_GET, [this]() {
        server_.send_P(200, "text/html; charset=utf-8", SETTINGS_PAGE);
    });
    server_.on("/logs", HTTP_GET, [this]() {
        server_.send_P(200, "text/html; charset=utf-8", LOG_PAGE);
    });
    server_.on("/api/config", HTTP_GET, [this]() { handleConfig(); });
    server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleSettingsUpdate(); });
    server_.on("/api/mapping", HTTP_POST, [this]() { handleMappingUpdate(); });
    server_.on("/api/mapping-list", HTTP_POST, [this]() {
        handleMappingListUpdate();
    });
    server_.on("/api/test-led", HTTP_POST, [this]() { handleTestLed(); });
    server_.on("/api/off", HTTP_POST, [this]() {
        allOff_();
        sendResult(true, "All LEDs switched off");
    });
    server_.onNotFound([this]() {
        server_.send(404, "application/json", "{\"ok\":false,\"message\":\"Not found\"}");
    });
    server_.begin();
}

void SettingsWebServer::handleClient()
{
    server_.handleClient();
}

void SettingsWebServer::handleConfig()
{
    if (settings_ == nullptr)
    {
        sendResult(false, "Settings are not initialized");
        return;
    }

    std::string json = "{\"board\":\"" + jsonEscape(boardName_) + "\"";
    json += ",\"boardRows\":" + std::to_string(boardRows_);
    json += ",\"logicalLedCount\":" +
        std::to_string(settings_->logicalMappingCount);
    json += ",\"physicalLedCount\":" +
        std::to_string(settings_->physicalLedCount);
    json += ",\"wledHost\":\"" + jsonEscape(settings_->wledHost) + "\"";
    json += ",\"segmentId\":" + std::to_string(settings_->segmentId);
    json += ",\"boulderBrightnessPercent\":" +
        std::to_string(settings_->boulderBrightnessPercent);
    json += ",\"aboveHoldBrightnessPercent\":" +
        std::to_string(settings_->aboveHoldBrightnessPercent);
    json += ",\"routeTimeoutMinutes\":" +
        std::to_string(settings_->routeTimeoutMinutes);
    json += ",\"kickerLedsEnabled\":" +
        std::string(settings_->kickerLedsEnabled ? "true" : "false");
    json += ",\"kickerColor\":{\"r\":" +
        std::to_string(settings_->kickerLedColor.red) +
        ",\"g\":" + std::to_string(settings_->kickerLedColor.green) +
        ",\"b\":" + std::to_string(settings_->kickerLedColor.blue) + "}";

    json += ",\"kickerIds\":[";
    for (size_t index = 0; index < settings_->kickerLedCount; ++index)
    {
        if (index > 0)
            json += ',';
        json += std::to_string(settings_->kickerLedIds[index]);
    }
    json += "],\"mapping\":[";
    for (size_t index = 0; index < settings_->logicalMappingCount; ++index)
    {
        if (index > 0)
            json += ',';
        json += std::to_string(settings_->logicalMapping[index]);
    }
    json += "]}";
    server_.send(200, "application/json", json.c_str());
}

void SettingsWebServer::handleLogs()
{
    if (liveLog_ == nullptr)
    {
        sendResult(false, "Live log is not initialized");
        return;
    }

    uint32_t afterSequence = 0;
    if (server_.hasArg("after"))
    {
        const String text = server_.arg("after");
        char *end = nullptr;
        const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
        if (
            text.length() == 0 ||
            text[0] == '-' ||
            end == text.c_str() ||
            *end != '\0')
        {
            sendResult(false, "Invalid log sequence");
            return;
        }
        afterSequence = static_cast<uint32_t>(parsed);
    }

    const std::string json = liveLog_->jsonSince(afterSequence);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", json.c_str());
}

bool SettingsWebServer::parseUnsignedArg(
    const char *name,
    uint32_t maximum,
    uint32_t &value,
    std::string &error)
{
    if (!server_.hasArg(name))
    {
        error = std::string("Missing field: ") + name;
        return false;
    }
    const String text = server_.arg(name);
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || parsed > maximum)
    {
        error = std::string("Invalid numeric field: ") + name;
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

void SettingsWebServer::handleSettingsUpdate()
{
    RuntimeSettings candidate = *settings_;
    std::string error;
    uint32_t value = 0;

    if (!server_.hasArg("host") || server_.arg("host").length() == 0)
    {
        sendResult(false, "WLED host is required");
        return;
    }
    const String host = server_.arg("host");
    if (host.length() >= sizeof(candidate.wledHost))
    {
        sendResult(false, "WLED host is too long");
        return;
    }
    std::strncpy(candidate.wledHost, host.c_str(), sizeof(candidate.wledHost));
    candidate.wledHost[sizeof(candidate.wledHost) - 1] = '\0';

    if (!parseUnsignedArg("physical", MAX_PHYSICAL_LED_COUNT, value, error))
        return sendResult(false, error);
    candidate.physicalLedCount = static_cast<uint16_t>(value);
    if (!parseUnsignedArg("segment", 255, value, error))
        return sendResult(false, error);
    candidate.segmentId = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("boulder", 100, value, error))
        return sendResult(false, error);
    candidate.boulderBrightnessPercent = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("above", 100, value, error))
        return sendResult(false, error);
    candidate.aboveHoldBrightnessPercent = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("timeout", 65535, value, error))
        return sendResult(false, error);
    candidate.routeTimeoutMinutes = static_cast<uint16_t>(value);

    candidate.kickerLedsEnabled =
        server_.hasArg("kickerEnabled") &&
        server_.arg("kickerEnabled") == "1";
    if (!server_.hasArg("kickerColor") ||
        !parseColor(server_.arg("kickerColor"), candidate.kickerLedColor))
    {
        sendResult(false, "Kicker color must use #RRGGBB format");
        return;
    }
    const String kickerIds = server_.hasArg("kickerIds")
        ? server_.arg("kickerIds")
        : String();
    if (!parseLedIdList(
            kickerIds.c_str(),
            candidate.kickerLedIds,
            MAX_KICKER_LED_COUNT,
            candidate.kickerLedCount,
            error))
    {
        sendResult(false, error);
        return;
    }

    if (!applySettings_(candidate, error))
        return sendResult(false, error);
    sendResult(true, "Settings saved");
}

void SettingsWebServer::handleMappingUpdate()
{
    RuntimeSettings candidate = *settings_;
    std::string error;
    uint32_t logical = 0;
    uint32_t physical = 0;
    if (!parseUnsignedArg(
            "logical",
            candidate.logicalMappingCount - 1,
            logical,
            error) ||
        !parseUnsignedArg(
            "physical",
            candidate.physicalLedCount - 1,
            physical,
            error))
    {
        sendResult(false, error);
        return;
    }
    candidate.logicalMapping[logical] = static_cast<uint16_t>(physical);
    if (!applySettings_(candidate, error))
        return sendResult(false, error);
    sendResult(true, "Mapping entry saved");
}

void SettingsWebServer::handleMappingListUpdate()
{
    if (!server_.hasArg("mapping"))
    {
        sendResult(false, "Mapping list is required");
        return;
    }
    RuntimeSettings candidate = *settings_;
    std::string error;
    uint16_t count = 0;
    if (!parseLedIdList(
            server_.arg("mapping").c_str(),
            candidate.logicalMapping,
            MAX_LOGICAL_LED_COUNT,
            count,
            error))
    {
        sendResult(false, error);
        return;
    }
    if (count != candidate.logicalMappingCount)
    {
        sendResult(false, "Mapping must contain exactly one ID per MoonBoard position");
        return;
    }
    if (!applySettings_(candidate, error))
        return sendResult(false, error);
    sendResult(true, "Complete mapping saved");
}

void SettingsWebServer::handleTestLed()
{
    std::string error;
    uint32_t physical = 0;
    if (!parseUnsignedArg(
            "physical",
            settings_->physicalLedCount - 1,
            physical,
            error))
    {
        sendResult(false, error);
        return;
    }
    if (!testLed_(static_cast<uint16_t>(physical), error))
        return sendResult(false, error);
    sendResult(true, "Test LED enabled");
}

void SettingsWebServer::sendResult(
    bool success,
    const std::string &message)
{
    const std::string json =
        std::string("{\"ok\":") + (success ? "true" : "false") +
        ",\"message\":\"" + jsonEscape(message.c_str()) + "\"}";
    server_.send(success ? 200 : 400, "application/json", json.c_str());
}
