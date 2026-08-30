#include "settings_web.h"

#include <Arduino.h>
#include <Update.h>
#include <esp_random.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "app_log.h"
#include "firmware_info.h"

namespace
{
constexpr uint32_t OTA_SESSION_TIMEOUT_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t OTA_RESTART_DELAY_MS = 1500;
constexpr size_t OTA_SESSION_RANDOM_BYTES = 24;

const char SETTINGS_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MoonBoard Bridge</title><style>
:root{font-family:system-ui,sans-serif;color:#17202a;background:#eef2f5}
body{max-width:850px;margin:auto;padding:16px}h1{margin:0 0 4px}.header{display:flex;justify-content:space-between;align-items:flex-start;gap:12px}
section{background:white;border-radius:12px;padding:18px;margin:16px 0;box-shadow:0 2px 10px #0001}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
label{display:flex;flex-direction:column;gap:5px;font-weight:600}input,textarea,button{font:inherit;padding:9px;border:1px solid #b8c2cc;border-radius:7px}
input[type=checkbox]{width:auto}label.check{flex-direction:row;align-items:center;margin-top:26px}
button{background:#175cd3;color:white;border:0;cursor:pointer}button.secondary{background:#536273}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}.status{min-height:24px;font-weight:600}.error{color:#b42318}.ok{color:#027a48}
textarea{min-height:105px;font-family:monospace;font-weight:400}.hint{color:#536273;font-size:.9rem}
.language{font-size:1.45rem;line-height:1;background:#fff;color:#17202a;border:1px solid #b8c2cc;padding:7px 9px;box-shadow:0 1px 4px #0001}footer{margin:22px 0 6px;text-align:center;color:#536273;font-size:.85rem}
</style></head><body><div class="header"><div><h1>MoonBoard Bridge</h1><div id="summary"></div></div><button id="languageToggle" class="language" type="button" aria-label="Switch language"></button></div>
<p><a href="/logs"><span data-en="Open live log" data-de="Live-Log öffnen">Open live log</span></a> · <a href="/ota"><span data-en="Update firmware" data-de="Firmware aktualisieren">Update firmware</span></a></p>
<section><h2><span data-en="Settings" data-de="Einstellungen">Settings</span></h2><form id="settings"><div class="grid">
<label><span data-en="WLED address" data-de="WLED-Adresse">WLED address</span><input id="host" required maxlength="63"></label>
<label><span data-en="Total physical LEDs" data-de="Gesamte physische LEDs">Total physical LEDs</span><input id="physical" type="number" min="1" max="2048" required></label>
<label><span data-en="WLED segment ID" data-de="WLED-Segment-ID">WLED segment ID</span><input id="segment" type="number" min="0" max="255" required></label>
<label><span data-en="Route brightness (%)" data-de="Routenhelligkeit (%)">Route brightness (%)</span><input id="boulder" type="number" min="0" max="100" required></label>
<label><span data-en="Additional light above (%)" data-de="Zusatzlicht oberhalb (%)">Additional light above (%)</span><input id="above" type="number" min="0" max="100" required></label>
<label><span data-en="Turn off after (minutes, 0 = never)" data-de="Abschaltung (Minuten, 0 = nie)">Turn off after (minutes, 0 = never)</span><input id="timeout" type="number" min="0" max="65535" required></label>
<label class="check"><input id="kickerEnabled" type="checkbox"><span data-en="Enable kicker LEDs with route" data-de="Kicker-LEDs mit Route einschalten">Enable kicker LEDs with route</span></label>
<label><span data-en="Kicker color" data-de="Kicker-Farbe">Kicker color</span><input id="kickerColor" type="color"></label>
<label style="grid-column:1/-1"><span data-en="Physical kicker LED IDs (CSV)" data-de="Physische Kicker-LED-IDs (CSV)">Physical kicker LED IDs (CSV)</span><input id="kickerIds" placeholder="0,3,6,9"></label>
</div><div class="actions"><button type="submit"><span data-en="Save settings" data-de="Einstellungen speichern">Save settings</span></button><button type="button" class="secondary" id="off"><span data-en="Switch off all LEDs" data-de="Alle LEDs aus">Switch off all LEDs</span></button></div></form></section>
<section><h2><span data-en="LED calibration" data-de="LED-Kalibrierung">LED calibration</span></h2><p class="hint"><span data-en="Each test LED switches off automatically after five seconds. The mapping is stored permanently after every assignment." data-de="Jede Test-LED wird nach fünf Sekunden automatisch ausgeschaltet. Das Mapping wird nach jeder Zuweisung dauerhaft gespeichert.">Each test LED switches off automatically after five seconds. The mapping is stored permanently after every assignment.</span></p>
<div class="grid"><label><span data-en="Logical position" data-de="Logische Position">Logical position</span><input id="logical" type="number" min="0"></label>
<label><span data-en="MoonBoard coordinate" data-de="MoonBoard-Koordinate">MoonBoard coordinate</span><input id="coordinate" readonly></label>
<label><span data-en="Physical WLED ID" data-de="Physische WLED-ID">Physical WLED ID</span><input id="mapped" type="number" min="0" max="2047"></label></div>
<div class="actions"><button type="button" class="secondary" id="previous"><span data-en="Previous" data-de="Zurück">Previous</span></button><button type="button" class="secondary" id="next"><span data-en="Next" data-de="Weiter">Next</span></button><button type="button" id="testLed"><span data-en="Test LED" data-de="LED testen">Test LED</span></button><button type="button" id="assign"><span data-en="Assign and save" data-de="Zuordnen und speichern">Assign and save</span></button></div>
<h3><span data-en="Import/export mapping" data-de="Mapping importieren/exportieren">Import/export mapping</span></h3><textarea id="mapping"></textarea>
<div class="actions"><button type="button" class="secondary" id="export"><span data-en="Show current mapping" data-de="Aktuelles Mapping anzeigen">Show current mapping</span></button><button type="button" id="import"><span data-en="Validate and save mapping" data-de="Mapping prüfen und speichern">Validate and save mapping</span></button></div></section>
<section><h2><span data-en="Configuration backup" data-de="Konfiguration sichern">Configuration backup</span></h2>
<p class="hint"><span data-en="Download all WLED, brightness, timeout, kicker and LED mapping settings as a JSON file. Restoring validates the complete file before replacing the current configuration. The OTA password is deliberately not included." data-de="Lade alle WLED-, Helligkeits-, Timeout-, Kicker- und LED-Mapping-Einstellungen als JSON-Datei herunter. Beim Wiederherstellen wird die vollständige Datei geprüft, bevor sie die aktuelle Konfiguration ersetzt. Das OTA-Passwort ist absichtlich nicht enthalten.">Download all WLED, brightness, timeout, kicker and LED mapping settings as a JSON file. Restoring validates the complete file before replacing the current configuration. The OTA password is deliberately not included.</span></p>
<div class="actions"><button type="button" class="secondary" id="backup"><span data-en="Download backup" data-de="Sicherung herunterladen">Download backup</span></button></div>
<div class="grid" style="margin-top:14px"><label><span data-en="JSON backup file" data-de="JSON-Sicherungsdatei">JSON backup file</span><input id="backupFile" type="file" accept=".json,application/json"></label></div>
<div class="actions"><button type="button" id="restore"><span data-en="Validate and restore" data-de="Prüfen und wiederherstellen">Validate and restore</span></button></div></section>
<div id="status" class="status"></div><p class="hint"><span data-en="Settings and the live log have no login. Firmware updates are protected by a separate password. Only expose this interface on a trusted local network." data-de="Einstellungen und Live-Log besitzen keine Anmeldung. Firmware-Updates sind separat passwortgeschützt. Die Oberfläche darf nur in einem vertrauenswürdigen lokalen Netzwerk erreichbar sein.">Settings and the live log have no login. Firmware updates are protected by a separate password. Only expose this interface on a trusted local network.</span></p>
<footer><span data-en="Firmware" data-de="Firmware">Firmware</span> <span id="firmwareVersion">—</span> · <span data-en="built" data-de="gebaut">built</span> <span id="firmwareBuild">—</span></footer>
<script>
const LANGUAGE_KEY='moonboardLanguage';let language=localStorage.getItem(LANGUAGE_KEY)==='de'?'de':'en',cfg;
const $=id=>document.getElementById(id);
const tr=(en,de)=>language==='de'?de:en;
function applyLanguage(){document.documentElement.lang=language;document.querySelectorAll('[data-en][data-de]').forEach(e=>e.textContent=e.dataset[language]);const b=$('languageToggle');b.textContent=language==='en'?'🇩🇪':'🇬🇧';b.title=language==='en'?'Auf Deutsch anzeigen':'Show in English';b.setAttribute('aria-label',b.title);if(cfg)renderSummary()}
function renderSummary(){$('summary').textContent=`${cfg.board} · ${cfg.logicalLedCount} ${tr('MoonBoard positions','MoonBoard-Positionen')}`}
function status(en,de,error=false){$('status').textContent=tr(en,de);$('status').className='status '+(error?'error':'ok')}
function colorHex(c){return '#'+[c.r,c.g,c.b].map(v=>v.toString(16).padStart(2,'0')).join('')}
function columnName(column){let s='';for(let n=column+1;n>0;n=Math.floor((n-1)/26))s=String.fromCharCode(65+(n-1)%26)+s;return s}
function coordinate(position){const col=Math.floor(position/cfg.boardRows),p=position%cfg.boardRows,row=col%2?cfg.boardRows-p:p+1;return columnName(col)+row}
function showMapping(){let i=Math.max(0,Math.min(cfg.logicalLedCount-1,Number($('logical').value)||0));$('logical').value=i;$('coordinate').value=coordinate(i);$('mapped').value=cfg.mapping[i]}
async function post(path,data){const response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)});const result=await response.json();if(!response.ok||!result.ok)throw new Error(result.message||tr('Error','Fehler'));return result}
async function load(){const response=await fetch('/api/config',{cache:'no-store'});cfg=await response.json();renderSummary();$('firmwareVersion').textContent=cfg.firmwareVersion;$('firmwareBuild').textContent=cfg.firmwareBuild;$('host').value=cfg.wledHost;$('physical').value=cfg.physicalLedCount;$('segment').value=cfg.segmentId;$('boulder').value=cfg.boulderBrightnessPercent;$('above').value=cfg.aboveHoldBrightnessPercent;$('timeout').value=cfg.routeTimeoutMinutes;$('kickerEnabled').checked=cfg.kickerLedsEnabled;$('kickerColor').value=colorHex(cfg.kickerColor);$('kickerIds').value=cfg.kickerIds.join(',');$('logical').max=cfg.logicalLedCount-1;showMapping()}
$('languageToggle').onclick=()=>{language=language==='en'?'de':'en';localStorage.setItem(LANGUAGE_KEY,language);applyLanguage()};applyLanguage();
$('settings').onsubmit=async e=>{e.preventDefault();try{await post('/api/settings',{host:$('host').value,physical:$('physical').value,segment:$('segment').value,boulder:$('boulder').value,above:$('above').value,timeout:$('timeout').value,kickerEnabled:$('kickerEnabled').checked?'1':'0',kickerColor:$('kickerColor').value,kickerIds:$('kickerIds').value});await load();status('Settings saved permanently.','Einstellungen dauerhaft gespeichert.')}catch(e){status(e.message,e.message,true)}};
$('logical').oninput=showMapping;$('previous').onclick=()=>{$('logical').value=Math.max(0,Number($('logical').value)-1);showMapping()};$('next').onclick=()=>{$('logical').value=Math.min(cfg.logicalLedCount-1,Number($('logical').value)+1);showMapping()};
$('testLed').onclick=async()=>{try{await post('/api/test-led',{physical:$('mapped').value});status('Test LED enabled for five seconds.','Test-LED für fünf Sekunden eingeschaltet.')}catch(e){status(e.message,e.message,true)}};
$('assign').onclick=async()=>{try{const logical=Number($('logical').value),physical=Number($('mapped').value);await post('/api/mapping',{logical,physical});cfg.mapping[logical]=physical;status(`${coordinate(logical)} was assigned to WLED ID ${physical}.`,`${coordinate(logical)} wurde WLED-ID ${physical} zugeordnet.`)}catch(e){status(e.message,e.message,true)}};
$('export').onclick=()=>{$('mapping').value=cfg.mapping.join(',');status('Mapping copied to the text field.','Mapping im Textfeld bereitgestellt.')};
$('import').onclick=async()=>{try{await post('/api/mapping-list',{mapping:$('mapping').value});await load();status('Complete mapping saved permanently.','Vollständiges Mapping dauerhaft gespeichert.')}catch(e){status(e.message,e.message,true)}};
$('off').onclick=async()=>{try{await post('/api/off',{});status('All LEDs switched off.','Alle LEDs ausgeschaltet.')}catch(e){status(e.message,e.message,true)}};
$('backup').onclick=()=>{const backup={format:'moonboard-bridge-config',schemaVersion:1,firmwareVersion:cfg.firmwareVersion,board:cfg.board,logicalLedCount:cfg.logicalLedCount,settings:{wledHost:cfg.wledHost,physicalLedCount:cfg.physicalLedCount,segmentId:cfg.segmentId,boulderBrightnessPercent:cfg.boulderBrightnessPercent,aboveHoldBrightnessPercent:cfg.aboveHoldBrightnessPercent,routeTimeoutMinutes:cfg.routeTimeoutMinutes,kickerLedsEnabled:cfg.kickerLedsEnabled,kickerColor:colorHex(cfg.kickerColor),kickerIds:cfg.kickerIds,mapping:cfg.mapping}};const blob=new Blob([JSON.stringify(backup,null,2)+'\n'],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a'),date=new Date().toISOString().slice(0,10),board=cfg.board.toLowerCase().replace(/[^a-z0-9_-]+/g,'-');a.href=url;a.download=`moonboard-${board}-config-${date}.json`;a.click();setTimeout(()=>URL.revokeObjectURL(url),0);status('Configuration backup downloaded.','Konfigurationssicherung heruntergeladen.')};
$('restore').onclick=async()=>{try{const file=$('backupFile').files[0];if(!file)throw new Error(tr('Select a JSON backup file first.','Wähle zuerst eine JSON-Sicherungsdatei aus.'));const backup=JSON.parse(await file.text()),s=backup&&backup.settings;if(backup.format!=='moonboard-bridge-config'||backup.schemaVersion!==1||!s||!Array.isArray(s.kickerIds)||!Array.isArray(s.mapping))throw new Error(tr('This is not a supported MoonBoard configuration backup.','Dies ist keine unterstützte MoonBoard-Konfigurationssicherung.'));if(!confirm(tr('Replace the current configuration with this backup? All LEDs will be switched off.','Aktuelle Konfiguration durch diese Sicherung ersetzen? Alle LEDs werden ausgeschaltet.')))return;await post('/api/restore',{backupSchema:backup.schemaVersion,board:backup.board,logicalCount:backup.logicalLedCount,host:s.wledHost,physical:s.physicalLedCount,segment:s.segmentId,boulder:s.boulderBrightnessPercent,above:s.aboveHoldBrightnessPercent,timeout:s.routeTimeoutMinutes,kickerEnabled:s.kickerLedsEnabled?'1':'0',kickerColor:s.kickerColor,kickerIds:s.kickerIds.join(','),mapping:s.mapping.join(',')});await load();status('Configuration restored and saved permanently.','Konfiguration wiederhergestellt und dauerhaft gespeichert.')}catch(e){status(e.message,e.message,true)}};
load().catch(e=>status(e.message,e.message,true));
</script></body></html>
)HTML";

const char LOG_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MoonBoard Live-Log</title><style>
:root{font-family:system-ui,sans-serif;color:#e6edf3;background:#0d1117}
body{max-width:1100px;margin:auto;padding:16px}a{color:#58a6ff}h1{margin:0 0 4px}.header{display:flex;justify-content:space-between;align-items:flex-start;gap:12px}
.toolbar{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin:16px 0}
button{font:inherit;padding:9px 14px;border:0;border-radius:7px;background:#238636;color:white;cursor:pointer}
label{display:flex;gap:6px;align-items:center}.state{font-weight:600;color:#3fb950}.error{color:#f85149}
pre{box-sizing:border-box;min-height:65vh;max-height:75vh;overflow:auto;white-space:pre-wrap;word-break:break-word;background:#010409;border:1px solid #30363d;border-radius:8px;padding:14px;margin:0;color:#c9d1d9}
.hint,footer{color:#8b949e;font-size:.9rem}.language{font-size:1.45rem;line-height:1;background:#161b22;border:1px solid #30363d;padding:7px 9px}footer{margin:22px 0 6px;text-align:center;font-size:.85rem}
</style></head><body><div class="header"><h1>MoonBoard Live Log</h1><button id="languageToggle" class="language" type="button" aria-label="Switch language"></button></div>
<p><a href="/"><span data-en="Back to settings and calibration" data-de="Zurück zu Einstellungen und Kalibrierung">Back to settings and calibration</span></a></p>
<div class="toolbar"><button id="clear"><span data-en="Clear display" data-de="Anzeige leeren">Clear display</span></button><button id="pause"></button>
<label><input id="scroll" type="checkbox" checked><span data-en="Auto-scroll" data-de="Automatisch scrollen">Auto-scroll</span></label><span id="state" class="state"></span></div>
<pre id="log"></pre><p class="hint"><span data-en="The latest 80 firmware messages are stored temporarily in RAM. A restart clears the buffer. This page refreshes once per second and has no login." data-de="Die letzten 80 Firmwaremeldungen liegen flüchtig im RAM. Ein Neustart löscht den Puffer. Die Seite aktualisiert sich einmal pro Sekunde und besitzt keine Anmeldung.">The latest 80 firmware messages are stored temporarily in RAM. A restart clears the buffer. This page refreshes once per second and has no login.</span></p>
<footer><span data-en="Firmware" data-de="Firmware">Firmware</span> <span id="firmwareVersion">—</span> · <span data-en="built" data-de="gebaut">built</span> <span id="firmwareBuild">—</span></footer>
<script>
const LANGUAGE_KEY='moonboardLanguage';let language=localStorage.getItem(LANGUAGE_KEY)==='de'?'de':'en',after=0,paused=false,busy=false;
const $=id=>document.getElementById(id),tr=(en,de)=>language==='de'?de:en,log=$('log'),state=$('state'),pause=$('pause');
function updateState(){if(paused){state.textContent=tr('Paused','Pausiert');return}state.textContent=tr('Connecting …','Verbinde …')}
function applyLanguage(){document.documentElement.lang=language;document.querySelectorAll('[data-en][data-de]').forEach(e=>e.textContent=e.dataset[language]);const b=$('languageToggle');b.textContent=language==='en'?'🇩🇪':'🇬🇧';b.title=language==='en'?'Auf Deutsch anzeigen':'Show in English';b.setAttribute('aria-label',b.title);pause.textContent=paused?tr('Resume','Fortsetzen'):tr('Pause','Pausieren');updateState()}
function append(entries){for(const entry of entries){const seconds=(entry.ms/1000).toFixed(3).padStart(10,' ');log.append(document.createTextNode(`[${seconds}s] ${entry.text}\n`))}while(log.childNodes.length>500)log.firstChild.remove();if(document.getElementById('scroll').checked)log.scrollTop=log.scrollHeight}
async function loadVersion(){try{const r=await fetch('/api/config',{cache:'no-store'}),c=await r.json();$('firmwareVersion').textContent=c.firmwareVersion;$('firmwareBuild').textContent=c.firmwareBuild}catch(e){}}
async function poll(){if(paused||busy)return;busy=true;try{const response=await fetch(`/api/logs?after=${after}`,{cache:'no-store'});const result=await response.json();if(!response.ok)throw new Error(result.message||tr('HTTP error','HTTP-Fehler'));append(result.entries);after=result.next;state.textContent=tr('Connected','Verbunden');state.className='state'}catch(error){state.textContent=`${tr('Not connected','Nicht verbunden')}: ${error.message}`;state.className='state error'}finally{busy=false}}
$('languageToggle').onclick=()=>{language=language==='en'?'de':'en';localStorage.setItem(LANGUAGE_KEY,language);applyLanguage()};$('clear').onclick=()=>{log.textContent=''};
pause.onclick=()=>{paused=!paused;applyLanguage();state.className='state';if(!paused)poll()};applyLanguage();loadVersion();
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

std::string htmlEscape(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

std::string bilingualText(
    const std::string &english,
    const std::string &german)
{
    return
        "<span data-en=\"" + htmlEscape(english) +
        "\" data-de=\"" + htmlEscape(german) + "\">" +
        htmlEscape(english) + "</span>";
}

std::string otaDocument(
    const std::string &englishTitle,
    const std::string &germanTitle,
    const std::string &content)
{
    return
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>" + htmlEscape(englishTitle) + "</title><style>"
        ":root{font-family:system-ui,sans-serif;color:#17202a;background:#eef2f5}"
        "body{max-width:620px;margin:auto;padding:16px}"
        ".header{display:flex;justify-content:space-between;align-items:flex-start;gap:12px}.header h1{margin:0}"
        "section{background:#fff;border-radius:12px;padding:20px;margin:16px 0;box-shadow:0 2px 10px #0001}"
        "label{display:flex;flex-direction:column;gap:5px;font-weight:600;margin:12px 0}"
        "input,button{box-sizing:border-box;width:100%;font:inherit;padding:10px;border:1px solid #b8c2cc;border-radius:7px}"
        "button{background:#175cd3;color:#fff;border:0;cursor:pointer;margin-top:8px}"
        "button.language{width:auto;font-size:1.45rem;line-height:1;background:#fff;color:#17202a;border:1px solid #b8c2cc;padding:7px 9px;margin:0;box-shadow:0 1px 4px #0001}"
        "button.secondary{background:#536273}.message{padding:10px;border-radius:7px;background:#fff3cd;color:#664d03}"
        ".error{background:#fef3f2;color:#b42318}.ok{background:#ecfdf3;color:#027a48}"
        ".drop-zone{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;min-height:92px;padding:16px;border:2px dashed #8fa3b8;border-radius:10px;background:#f8fafc;text-align:center;transition:border-color .15s,background .15s}"
        ".drop-zone.drag-over{border-color:#175cd3;background:#eaf2ff}.drop-zone.invalid{border-color:#b42318;background:#fef3f2;color:#b42318}"
        ".drop-zone strong{font-size:1.05rem}.drop-zone .hint{margin:0}"
        ".hint{color:#536273;font-size:.9rem}a{color:#175cd3}footer{margin:22px 0 6px;text-align:center;color:#536273;font-size:.85rem}"
        "</style></head><body data-title-en=\"" +
        htmlEscape(englishTitle) + "\" data-title-de=\"" +
        htmlEscape(germanTitle) + "\"><div class=\"header\"><h1>" +
        bilingualText("MoonBoard Firmware Update", "MoonBoard Firmware-Update") +
        "</h1><button id=\"languageToggle\" class=\"language\" type=\"button\" aria-label=\"Switch language\"></button></div>" +
        content + "<footer>" +
        bilingualText("Firmware", "Firmware") + " " +
        htmlEscape(FIRMWARE_VERSION) + " · " +
        bilingualText("built", "gebaut") + " " +
        htmlEscape(FIRMWARE_BUILD_TIMESTAMP) +
        "</footer><script>"
        "const LANGUAGE_KEY='moonboardLanguage';"
        "let language=localStorage.getItem(LANGUAGE_KEY)==='de'?'de':'en';"
        "let selectedFirmwareName='',firmwareFileError='';"
        "const tr=(en,de)=>language==='de'?de:en;"
        "const firmwareInput=document.getElementById('firmwareFile');"
        "const dropZone=document.getElementById('firmwareDropZone');"
        "const dropStatus=document.getElementById('firmwareDropStatus');"
        "function renderFirmwareStatus(){if(!dropStatus)return;dropZone.classList.toggle('invalid',!!firmwareFileError);if(firmwareFileError==='count')dropStatus.textContent=tr('Drop exactly one .bin firmware file.','Ziehe genau eine .bin-Firmwaredatei hierher.');else if(firmwareFileError==='type')dropStatus.textContent=tr('Only a .bin firmware file is accepted.','Es wird nur eine .bin-Firmwaredatei akzeptiert.');else if(firmwareFileError==='assign')dropStatus.textContent=tr('The dropped file could not be selected. Use Browse instead.','Die abgelegte Datei konnte nicht ausgewählt werden. Nutze stattdessen Durchsuchen.');else if(selectedFirmwareName)dropStatus.textContent=tr('Selected: ','Ausgewählt: ')+selectedFirmwareName;else dropStatus.textContent=tr('Drop firmware.bin into this browser window.','Ziehe firmware.bin in dieses Browserfenster.')}"
        "function validateFirmwareFile(file){firmwareFileError='';selectedFirmwareName='';if(!file){firmwareFileError='count';renderFirmwareStatus();return false}if(!file.name.toLowerCase().endsWith('.bin')){firmwareFileError='type';if(firmwareInput)firmwareInput.value='';renderFirmwareStatus();return false}selectedFirmwareName=file.name;renderFirmwareStatus();return true}"
        "function applyLanguage(){document.documentElement.lang=language;document.title=document.body.dataset[language==='de'?'titleDe':'titleEn'];document.querySelectorAll('[data-en][data-de]').forEach(e=>e.textContent=e.dataset[language]);const b=document.getElementById('languageToggle');b.textContent=language==='en'?'🇩🇪':'🇬🇧';b.title=language==='en'?'Auf Deutsch anzeigen':'Show in English';b.setAttribute('aria-label',b.title);renderFirmwareStatus()}"
        "document.getElementById('languageToggle').onclick=()=>{language=language==='en'?'de':'en';localStorage.setItem(LANGUAGE_KEY,language);applyLanguage()};"
        "if(firmwareInput)firmwareInput.onchange=()=>validateFirmwareFile(firmwareInput.files[0]);"
        "if(dropZone){let dragDepth=0;document.addEventListener('dragenter',e=>{e.preventDefault();dragDepth++;dropZone.classList.add('drag-over')});document.addEventListener('dragover',e=>e.preventDefault());document.addEventListener('dragleave',e=>{e.preventDefault();dragDepth=Math.max(0,dragDepth-1);if(!dragDepth)dropZone.classList.remove('drag-over')});document.addEventListener('drop',e=>{e.preventDefault();dragDepth=0;dropZone.classList.remove('drag-over');const files=e.dataTransfer&&e.dataTransfer.files;if(!files||files.length!==1){firmwareFileError='count';selectedFirmwareName='';firmwareInput.value='';renderFirmwareStatus();return}if(!files[0].name.toLowerCase().endsWith('.bin')){firmwareFileError='type';selectedFirmwareName='';firmwareInput.value='';renderFirmwareStatus();return}try{firmwareInput.files=files}catch(error){firmwareFileError='assign';selectedFirmwareName='';firmwareInput.value='';renderFirmwareStatus();return}validateFirmwareFile(firmwareInput.files[0])})}"
        "const upload=document.getElementById('upload');if(upload)upload.onsubmit=e=>{if(!validateFirmwareFile(firmwareInput&&firmwareInput.files[0])){e.preventDefault();return}const b=document.getElementById('uploadButton');b.disabled=true;b.textContent=tr('Upload in progress …','Upload läuft …')};"
        "applyLanguage();</script></body></html>";
}

std::string englishOtaAuthError(const std::string &german)
{
    if (german == "Das Passwort muss 8 bis 64 Zeichen lang sein")
        return "The password must be 8 to 64 characters long";
    if (german == "Das Passwort konnte nicht verarbeitet werden")
        return "The password could not be processed";
    if (german == "Der OTA-Passwortspeicher konnte nicht geöffnet werden")
        return "The OTA password storage could not be opened";
    if (german == "Das OTA-Passwort konnte nicht dauerhaft gespeichert werden")
        return "The OTA password could not be stored permanently";
    return german;
}

std::string germanOtaUploadError(const std::string &english)
{
    if (english == "The session has expired")
        return "Die Anmeldung ist abgelaufen";
    if (english == "A restart is already pending")
        return "Ein Neustart ist bereits geplant";
    if (english == "Please select a PlatformIO firmware.bin file")
        return "Bitte eine PlatformIO-firmware.bin auswählen";
    if (english == "The OTA update could not be started")
        return "Das OTA-Update konnte nicht gestartet werden";
    if (english == "The firmware could not be written completely")
        return "Die Firmware konnte nicht vollständig geschrieben werden";
    if (english == "Firmware verification failed")
        return "Die Firmwareprüfung ist fehlgeschlagen";
    if (english == "The upload was aborted")
        return "Der Upload wurde abgebrochen";
    if (english == "No firmware file was received")
        return "Es wurde keine Firmwaredatei empfangen";
    return english;
}

std::string randomHexToken()
{
    uint8_t randomBytes[OTA_SESSION_RANDOM_BYTES];
    esp_fill_random(randomBytes, sizeof(randomBytes));
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string token;
    token.reserve(sizeof(randomBytes) * 2);
    for (const uint8_t value : randomBytes)
    {
        token += HEX_DIGITS[value >> 4];
        token += HEX_DIGITS[value & 0x0F];
    }
    return token;
}

std::string cookieValue(const std::string &cookies, const char *name)
{
    const std::string prefix = std::string(name) + '=';
    size_t start = 0;
    while (start < cookies.size())
    {
        const size_t end = cookies.find(';', start);
        size_t fieldStart = start;
        while (fieldStart < cookies.size() && cookies[fieldStart] == ' ')
            ++fieldStart;
        const size_t fieldEnd =
            end == std::string::npos ? cookies.size() : end;
        if (cookies.compare(fieldStart, prefix.size(), prefix) == 0)
        {
            return cookies.substr(
                fieldStart + prefix.size(),
                fieldEnd - fieldStart - prefix.size());
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return std::string();
}

bool constantTimeEquals(
    const std::string &candidate,
    const std::string &expected)
{
    if (candidate.size() != expected.size())
        return false;
    uint8_t difference = 0;
    for (size_t index = 0; index < expected.size(); ++index)
        difference |= candidate[index] ^ expected[index];
    return difference == 0;
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
      boardRows_(0),
      otaSessionLastSeenMs_(0),
      otaUploadAuthorized_(false),
      otaUploadStarted_(false),
      otaUploadSucceeded_(false),
      otaUploadSize_(0),
      otaRestartPending_(false),
      otaRestartStartedAtMs_(0)
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

    std::string otaAuthError;
    if (!otaAuth_.load(otaAuthError))
        appLogPrintf("[OTA] %s\n", otaAuthError.c_str());
    else if (otaAuth_.configured())
        appLogLine("[OTA] Password protection loaded from NVS");
    else
        appLogLine("[OTA] Password will be set on the first OTA-page visit");

    const char *collectedHeaders[] = {"Cookie"};
    server_.collectHeaders(collectedHeaders, 1);

    server_.on("/", HTTP_GET, [this]() {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send_P(200, "text/html; charset=utf-8", SETTINGS_PAGE);
    });
    server_.on("/logs", HTTP_GET, [this]() {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send_P(200, "text/html; charset=utf-8", LOG_PAGE);
    });
    server_.on("/api/config", HTTP_GET, [this]() { handleConfig(); });
    server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleSettingsUpdate(); });
    server_.on("/api/restore", HTTP_POST, [this]() {
        handleConfigurationRestore();
    });
    server_.on("/api/mapping", HTTP_POST, [this]() { handleMappingUpdate(); });
    server_.on("/api/mapping-list", HTTP_POST, [this]() {
        handleMappingListUpdate();
    });
    server_.on("/api/test-led", HTTP_POST, [this]() { handleTestLed(); });
    server_.on("/api/off", HTTP_POST, [this]() {
        allOff_();
        sendResult(true, "All LEDs switched off");
    });
    server_.on("/ota", HTTP_GET, [this]() { handleOtaPage(); });
    server_.on("/ota/setup", HTTP_POST, [this]() { handleOtaSetup(); });
    server_.on("/ota/login", HTTP_POST, [this]() { handleOtaLogin(); });
    server_.on("/ota/logout", HTTP_POST, [this]() { handleOtaLogout(); });
    server_.on("/ota/password", HTTP_POST, [this]() {
        handleOtaPasswordChange();
    });
    server_.on(
        "/ota/upload",
        HTTP_POST,
        [this]() { handleOtaUploadComplete(); },
        [this]() { handleOtaUploadData(); });
    server_.onNotFound([this]() {
        server_.send(404, "application/json", "{\"ok\":false,\"message\":\"Not found\"}");
    });
    server_.begin();
}

void SettingsWebServer::handleClient()
{
    server_.handleClient();
    if (
        otaRestartPending_ &&
        static_cast<uint32_t>(millis()) - otaRestartStartedAtMs_ >=
            OTA_RESTART_DELAY_MS)
    {
        appLogLine("[OTA] Restarting into the new firmware");
        delay(50);
        ESP.restart();
    }
}

void SettingsWebServer::handleConfig()
{
    if (settings_ == nullptr)
    {
        sendResult(false, "Settings are not initialized");
        return;
    }

    std::string json = "{\"board\":\"" + jsonEscape(boardName_) + "\"";
    json += ",\"firmwareVersion\":\"" +
        jsonEscape(FIRMWARE_VERSION) + "\"";
    json += ",\"firmwareBuild\":\"" +
        jsonEscape(FIRMWARE_BUILD_TIMESTAMP) + "\"";
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
    server_.sendHeader("Cache-Control", "no-store");
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

bool SettingsWebServer::parseSettingsArgs(
    RuntimeSettings &candidate,
    std::string &error)
{
    uint32_t value = 0;

    if (!server_.hasArg("host") || server_.arg("host").length() == 0)
    {
        error = "WLED host is required";
        return false;
    }
    const String host = server_.arg("host");
    if (host.length() >= sizeof(candidate.wledHost))
    {
        error = "WLED host is too long";
        return false;
    }
    std::strncpy(candidate.wledHost, host.c_str(), sizeof(candidate.wledHost));
    candidate.wledHost[sizeof(candidate.wledHost) - 1] = '\0';

    if (!parseUnsignedArg("physical", MAX_PHYSICAL_LED_COUNT, value, error))
        return false;
    candidate.physicalLedCount = static_cast<uint16_t>(value);
    if (!parseUnsignedArg("segment", 255, value, error))
        return false;
    candidate.segmentId = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("boulder", 100, value, error))
        return false;
    candidate.boulderBrightnessPercent = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("above", 100, value, error))
        return false;
    candidate.aboveHoldBrightnessPercent = static_cast<uint8_t>(value);
    if (!parseUnsignedArg("timeout", 65535, value, error))
        return false;
    candidate.routeTimeoutMinutes = static_cast<uint16_t>(value);

    candidate.kickerLedsEnabled =
        server_.hasArg("kickerEnabled") &&
        server_.arg("kickerEnabled") == "1";
    if (!server_.hasArg("kickerColor") ||
        !parseColor(server_.arg("kickerColor"), candidate.kickerLedColor))
    {
        error = "Kicker color must use #RRGGBB format";
        return false;
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
        return false;
    }

    error.clear();
    return true;
}

void SettingsWebServer::handleSettingsUpdate()
{
    RuntimeSettings candidate = *settings_;
    std::string error;
    if (!parseSettingsArgs(candidate, error))
        return sendResult(false, error);

    if (!applySettings_(candidate, error))
        return sendResult(false, error);
    sendResult(true, "Settings saved");
}

void SettingsWebServer::handleConfigurationRestore()
{
    if (settings_ == nullptr)
    {
        sendResult(false, "Settings are not initialized");
        return;
    }

    std::string error;
    uint32_t value = 0;
    if (!parseUnsignedArg("backupSchema", 1, value, error) || value != 1)
    {
        sendResult(false, "Unsupported backup schema");
        return;
    }
    if (!server_.hasArg("board") || server_.arg("board") != boardName_)
    {
        sendResult(false, "Backup belongs to a different board layout");
        return;
    }
    if (!parseUnsignedArg(
            "logicalCount",
            MAX_LOGICAL_LED_COUNT,
            value,
            error) ||
        value != settings_->logicalMappingCount)
    {
        sendResult(
            false,
            "Backup has an incompatible MoonBoard position count");
        return;
    }

    RuntimeSettings candidate = *settings_;
    if (!parseSettingsArgs(candidate, error))
        return sendResult(false, error);
    if (!server_.hasArg("mapping"))
    {
        sendResult(false, "Backup mapping is missing");
        return;
    }

    uint16_t mappingCount = 0;
    if (!parseLedIdList(
            server_.arg("mapping").c_str(),
            candidate.logicalMapping,
            MAX_LOGICAL_LED_COUNT,
            mappingCount,
            error))
    {
        return sendResult(false, error);
    }
    if (mappingCount != candidate.logicalMappingCount)
    {
        sendResult(
            false,
            "Backup mapping must contain exactly one ID per position");
        return;
    }

    if (!applySettings_(candidate, error))
        return sendResult(false, error);
    appLogLine("[SETTINGS] Configuration restored from JSON backup");
    sendResult(true, "Configuration restored");
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

void SettingsWebServer::handleOtaPage()
{
    if (!otaAuth_.configured())
    {
        sendOtaSetupPage(std::string(), std::string(), 200);
        return;
    }
    if (!hasValidOtaSession())
    {
        sendOtaLoginPage(std::string(), std::string(), 200);
        return;
    }
    sendOtaUploadPage(std::string(), std::string(), 200);
}

void SettingsWebServer::handleOtaSetup()
{
    if (otaAuth_.configured())
    {
        redirectToOta();
        return;
    }
    const std::string setupToken = server_.hasArg("setupToken")
        ? std::string(server_.arg("setupToken").c_str())
        : std::string();
    if (
        otaSetupToken_.empty() ||
        !constantTimeEquals(setupToken, otaSetupToken_))
    {
        otaSetupToken_ = randomHexToken();
        sendOtaSetupPage(
            "Invalid or expired initial setup.",
            "Ungültige oder abgelaufene Ersteinrichtung.",
            403);
        return;
    }
    if (!server_.hasArg("password") || !server_.hasArg("confirmation"))
    {
        sendOtaSetupPage(
            "Please fill in both password fields.",
            "Bitte beide Passwortfelder ausfüllen.",
            400);
        return;
    }

    const std::string password = server_.arg("password").c_str();
    const std::string confirmation = server_.arg("confirmation").c_str();
    if (password != confirmation)
    {
        sendOtaSetupPage(
            "The passwords do not match.",
            "Die Passwörter stimmen nicht überein.",
            400);
        return;
    }

    std::string error;
    if (!otaAuth_.setPassword(password, error))
    {
        sendOtaSetupPage(englishOtaAuthError(error), error, 400);
        return;
    }

    std::fill(otaSetupToken_.begin(), otaSetupToken_.end(), '\0');
    otaSetupToken_.clear();
    beginOtaSession();
    appLogLine("[OTA] Initial password saved to NVS");
    redirectToOta();
}

void SettingsWebServer::handleOtaLogin()
{
    if (!otaAuth_.configured())
    {
        sendOtaSetupPage(std::string(), std::string(), 200);
        return;
    }

    const std::string password = server_.hasArg("password")
        ? std::string(server_.arg("password").c_str())
        : std::string();
    if (!otaAuth_.verifyPassword(password))
    {
        appLogLine("[OTA] Rejected login attempt");
        delay(350);
        sendOtaLoginPage("Incorrect password.", "Falsches Passwort.", 401);
        return;
    }

    beginOtaSession();
    appLogLine("[OTA] Login successful");
    redirectToOta();
}

void SettingsWebServer::handleOtaLogout()
{
    if (hasValidOtaSession(false))
        clearOtaSession();
    server_.sendHeader(
        "Set-Cookie",
        "ota_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    server_.sendHeader("Location", "/ota");
    server_.send(303, "text/plain", "");
}

void SettingsWebServer::handleOtaPasswordChange()
{
    if (!otaAuth_.configured() || !hasValidOtaSession())
    {
        sendOtaLoginPage(
            "The session has expired.",
            "Die Anmeldung ist abgelaufen.",
            401);
        return;
    }
    if (!server_.hasArg("password") || !server_.hasArg("confirmation"))
    {
        sendOtaUploadPage(
            "Please fill in both new password fields.",
            "Bitte beide neuen Passwortfelder ausfüllen.",
            400);
        return;
    }

    const std::string password = server_.arg("password").c_str();
    const std::string confirmation = server_.arg("confirmation").c_str();
    if (password != confirmation)
    {
        sendOtaUploadPage(
            "The new passwords do not match.",
            "Die neuen Passwörter stimmen nicht überein.",
            400);
        return;
    }

    std::string error;
    if (!otaAuth_.setPassword(password, error))
    {
        sendOtaUploadPage(englishOtaAuthError(error), error, 400);
        return;
    }

    appLogLine("[OTA] Password changed");
    sendOtaMessagePage(
        "Password changed",
        "Passwort geändert",
        "The new OTA password was stored permanently.",
        "Das neue OTA-Passwort wurde dauerhaft gespeichert.",
        200);
}

void SettingsWebServer::handleOtaUploadData()
{
    HTTPUpload &upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        otaUploadAuthorized_ =
            otaAuth_.configured() && hasValidOtaSession();
        otaUploadStarted_ = false;
        otaUploadSucceeded_ = false;
        otaUploadError_.clear();
        otaUploadSize_ = 0;

        if (!otaUploadAuthorized_)
        {
            otaUploadError_ = "The session has expired";
            return;
        }
        if (otaRestartPending_)
        {
            otaUploadError_ = "A restart is already pending";
            return;
        }

        String filename = upload.filename;
        filename.toLowerCase();
        if (filename.length() == 0 || !filename.endsWith(".bin"))
        {
            otaUploadError_ = "Please select a PlatformIO firmware.bin file";
            return;
        }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            otaUploadError_ = Update.errorString();
            if (otaUploadError_.empty())
                otaUploadError_ = "The OTA update could not be started";
            return;
        }

        otaUploadStarted_ = true;
        appLogPrintf(
            "[OTA] Firmware upload started: %s\n",
            upload.filename.c_str());
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!otaUploadStarted_ || !otaUploadError_.empty())
            return;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            otaUploadError_ = Update.errorString();
            if (otaUploadError_.empty())
                otaUploadError_ =
                    "The firmware could not be written completely";
            Update.abort();
            otaUploadStarted_ = false;
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_END)
    {
        otaUploadSize_ = upload.totalSize;
        if (!otaUploadStarted_ || !otaUploadError_.empty())
            return;
        if (!Update.end(true))
        {
            otaUploadError_ = Update.errorString();
            if (otaUploadError_.empty())
                otaUploadError_ = "Firmware verification failed";
            otaUploadStarted_ = false;
            return;
        }
        otaUploadStarted_ = false;
        otaUploadSucceeded_ = true;
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        if (Update.isRunning())
            Update.abort();
        otaUploadStarted_ = false;
        otaUploadSucceeded_ = false;
        otaUploadError_ = "The upload was aborted";
    }
}

void SettingsWebServer::handleOtaUploadComplete()
{
    if (!otaUploadAuthorized_)
    {
        sendOtaLoginPage(
            "The session has expired.",
            "Die Anmeldung ist abgelaufen.",
            403);
        return;
    }

    if (!otaUploadSucceeded_)
    {
        if (otaUploadError_.empty())
            otaUploadError_ = "No firmware file was received";
        appLogPrintf("[OTA] Upload failed: %s\n", otaUploadError_.c_str());
        sendOtaUploadPage(
            otaUploadError_,
            germanOtaUploadError(otaUploadError_),
            400);
        return;
    }

    appLogPrintf(
        "[OTA] Firmware verified: %u bytes\n",
        static_cast<unsigned>(otaUploadSize_));
    sendOtaMessagePage(
        "Update successful",
        "Update erfolgreich",
        "The firmware was verified and stored. The Olimex is restarting now. Reload the page afterwards and verify the version in the footer.",
        "Die Firmware wurde geprüft und gespeichert. Der Olimex startet jetzt neu. Lade die Seite anschließend neu und prüfe die Version im Footer.",
        200);
    otaRestartPending_ = true;
    otaRestartStartedAtMs_ = static_cast<uint32_t>(millis());
}

void SettingsWebServer::sendOtaSetupPage(
    const std::string &englishMessage,
    const std::string &germanMessage,
    int statusCode)
{
    if (otaSetupToken_.empty())
        otaSetupToken_ = randomHexToken();
    std::string content =
        "<section><h2>" +
        bilingualText(
            "Set the initial OTA password",
            "OTA-Passwort erstmalig festlegen") +
        "</h2><p>" +
        bilingualText(
            "A password must be set once before the first firmware update. It is stored in NVS and remains valid after subsequent updates.",
            "Vor dem ersten Firmware-Update muss einmalig ein Passwort gesetzt werden. Es bleibt in NVS gespeichert und gilt auch nach späteren Updates.") +
        "</p>";
    if (!englishMessage.empty() || !germanMessage.empty())
    {
        content += "<p class=\"message error\">" +
            bilingualText(englishMessage, germanMessage) + "</p>";
    }
    content +=
        "<form method=\"post\" action=\"/ota/setup\">"
        "<input type=\"hidden\" name=\"setupToken\" value=\"" +
        otaSetupToken_ + "\">"
        "<label>" + bilingualText("New password", "Neues Passwort") +
        "<input type=\"password\" name=\"password\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\" required></label>"
        "<label>" + bilingualText("Repeat password", "Passwort wiederholen") +
        "<input type=\"password\" name=\"confirmation\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\" required></label>"
        "<button type=\"submit\">" +
        bilingualText("Save password", "Passwort speichern") +
        "</button></form><p class=\"hint\">" +
        bilingualText(
            "Only a salted PBKDF2-SHA-256 verifier is stored. Because this page uses HTTP, use it only on a trusted local network.",
            "Das Passwort wird nur als gesalzener PBKDF2-SHA-256-Prüfwert gespeichert. Da die Seite HTTP verwendet, darf sie nur in einem vertrauenswürdigen lokalen Netzwerk benutzt werden.") +
        "</p></section><p><a href=\"/\">" +
        bilingualText("Back to settings", "Zurück zu den Einstellungen") +
        "</a></p>";
    sendOtaHtml(
        "Set OTA Password",
        "OTA-Passwort festlegen",
        content,
        statusCode);
}

void SettingsWebServer::sendOtaLoginPage(
    const std::string &englishMessage,
    const std::string &germanMessage,
    int statusCode)
{
    std::string content = "<section><h2>" +
        bilingualText("Sign in", "Anmelden") + "</h2>";
    if (!englishMessage.empty() || !germanMessage.empty())
    {
        content += "<p class=\"message error\">" +
            bilingualText(englishMessage, germanMessage) + "</p>";
    }
    content +=
        "<form method=\"post\" action=\"/ota/login\">"
        "<label>" + bilingualText("OTA password", "OTA-Passwort") +
        "<input type=\"password\" name=\"password\" maxlength=\"64\" autocomplete=\"current-password\" required autofocus></label>"
        "<button type=\"submit\">" + bilingualText("Sign in", "Anmelden") +
        "</button></form><p class=\"hint\">" +
        bilingualText(
            "The session remains valid for 30 minutes after the last activity.",
            "Die Anmeldung bleibt 30 Minuten ab der letzten Aktivität gültig.") +
        "</p></section><p><a href=\"/\">" +
        bilingualText("Back to settings", "Zurück zu den Einstellungen") +
        "</a></p>";
    sendOtaHtml("OTA Sign-in", "OTA-Anmeldung", content, statusCode);
}

void SettingsWebServer::sendOtaUploadPage(
    const std::string &englishMessage,
    const std::string &germanMessage,
    int statusCode)
{
    std::string content = "<section><h2>" +
        bilingualText("Upload firmware", "Firmware hochladen") + "</h2>";
    if (!englishMessage.empty() || !germanMessage.empty())
    {
        content += "<p class=\"message error\">" +
            bilingualText(englishMessage, germanMessage) + "</p>";
    }
    content +=
        "<p>" +
        bilingualText(
            "Select only the file generated by PlatformIO:",
            "Wähle ausschließlich die von PlatformIO erzeugte Datei:") +
        " <code>.pio/build/olimex-esp32-poe-iso/firmware.bin</code>. " +
        bilingualText(
            "Bootloader and partition files are not suitable here.",
            "Bootloader- oder Partitionsdateien sind hier nicht geeignet.") +
        "</p><p class=\"hint\">" +
        bilingualText("Available OTA slot: about", "Verfügbarer OTA-Slot: etwa") +
        " " +
        std::to_string(ESP.getFreeSketchSpace() / 1024) +
        " KiB. " +
        bilingualText(
            "Do not disconnect power or network during the upload.",
            "Strom und Netzwerk während des Uploads nicht trennen.") +
        "</p>"
        "<form id=\"upload\" method=\"post\" action=\"/ota/upload\" enctype=\"multipart/form-data\">"
        "<div id=\"firmwareDropZone\" class=\"drop-zone\"><strong id=\"firmwareDropStatus\" aria-live=\"polite\"></strong><span class=\"hint\">" +
        bilingualText(
            "or use Browse below.",
            "oder nutze unten Durchsuchen.") +
        "</span></div>"
        "<label>" + bilingualText("Firmware file", "Firmwaredatei") +
        "<input id=\"firmwareFile\" type=\"file\" name=\"firmware\" accept=\".bin,application/octet-stream\" required></label>"
        "<button id=\"uploadButton\" type=\"submit\">" +
        bilingualText("Upload firmware", "Firmware hochladen") +
        "</button></form></section><section><h2>" +
        bilingualText("Change OTA password", "OTA-Passwort ändern") +
        "</h2>"
        "<form method=\"post\" action=\"/ota/password\">"
        "<label>" + bilingualText("New password", "Neues Passwort") +
        "<input type=\"password\" name=\"password\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\" required></label>"
        "<label>" + bilingualText("Repeat password", "Passwort wiederholen") +
        "<input type=\"password\" name=\"confirmation\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\" required></label>"
        "<button type=\"submit\" class=\"secondary\">" +
        bilingualText("Change password", "Passwort ändern") +
        "</button></form></section><form method=\"post\" action=\"/ota/logout\"><button type=\"submit\" class=\"secondary\">" +
        bilingualText("Sign out", "Abmelden") +
        "</button></form><p><a href=\"/\">" +
        bilingualText("Back to settings", "Zurück zu den Einstellungen") +
        "</a></p>";
    sendOtaHtml(
        "Upload Firmware",
        "Firmware hochladen",
        content,
        statusCode);
}

void SettingsWebServer::sendOtaMessagePage(
    const std::string &englishTitle,
    const std::string &germanTitle,
    const std::string &englishMessage,
    const std::string &germanMessage,
    int statusCode)
{
    const std::string content =
        "<section><h2>" + bilingualText(englishTitle, germanTitle) +
        "</h2><p class=\"message ok\">" +
        bilingualText(englishMessage, germanMessage) +
        "</p><p><a href=\"/ota\">" +
        bilingualText(
            "Back to firmware update",
            "Zurück zum Firmware-Update") +
        "</a></p></section>";
    sendOtaHtml(englishTitle, germanTitle, content, statusCode);
}

void SettingsWebServer::sendOtaHtml(
    const std::string &englishTitle,
    const std::string &germanTitle,
    const std::string &content,
    int statusCode)
{
    const std::string page = otaDocument(
        englishTitle,
        germanTitle,
        content);
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    server_.send(statusCode, "text/html; charset=utf-8", page.c_str());
}

void SettingsWebServer::beginOtaSession()
{
    otaSessionToken_ = randomHexToken();
    otaSessionLastSeenMs_ = static_cast<uint32_t>(millis());
}

void SettingsWebServer::clearOtaSession()
{
    std::fill(
        otaSessionToken_.begin(),
        otaSessionToken_.end(),
        '\0');
    otaSessionToken_.clear();
    otaSessionLastSeenMs_ = 0;
}

bool SettingsWebServer::hasValidOtaSession(bool refresh)
{
    if (otaSessionToken_.empty())
        return false;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (now - otaSessionLastSeenMs_ >= OTA_SESSION_TIMEOUT_MS)
    {
        clearOtaSession();
        return false;
    }

    const String cookieHeader = server_.header("Cookie");
    const std::string supplied = cookieValue(
        std::string(cookieHeader.c_str(), cookieHeader.length()),
        "ota_session");
    if (!constantTimeEquals(supplied, otaSessionToken_))
        return false;
    if (refresh)
        otaSessionLastSeenMs_ = now;
    return true;
}

void SettingsWebServer::redirectToOta()
{
    if (!otaSessionToken_.empty())
    {
        const std::string cookie =
            "ota_session=" + otaSessionToken_ +
            "; Path=/; HttpOnly; SameSite=Strict";
        server_.sendHeader("Set-Cookie", cookie.c_str());
    }
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("Location", "/ota");
    server_.send(303, "text/plain", "");
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
