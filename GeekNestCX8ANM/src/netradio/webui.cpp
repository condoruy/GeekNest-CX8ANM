#include "webui.h"
#include "actions.h"
#include "config.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
WebServer server(80);
DNSServer dns;
bool captive = false;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CX8ANM Radio</title>
<style>
:root{--bg:#0b0f14;--panel:#121a24;--line:#243040;--text:#e6e9ee;--dim:#8a93a0;--acc:#ffa31a;--teal:#2bb5f5;--ok:#36c26b;--bad:#f04a4a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:10px;padding:14px 16px;background:var(--panel);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:2}
header b{color:var(--acc);font-size:18px;letter-spacing:.5px}header span{color:var(--dim);font-size:13px}
.chip{margin-left:auto;font-size:12px;padding:3px 9px;border-radius:99px;border:1px solid var(--line);color:var(--dim)}
main{max-width:640px;margin:0 auto;padding:12px 16px 40px}
section{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px;margin:12px 0}
h2{margin:0 0 10px;font-size:12px;letter-spacing:1px;color:var(--dim);font-weight:600;text-transform:uppercase}
.st{font-size:22px;font-weight:700}.ti{color:var(--text);min-height:22px;margin:6px 0}.sm{color:var(--dim);font-size:13px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button{background:#1b2633;color:var(--text);border:1px solid var(--line);border-radius:9px;padding:9px 13px;font:inherit;cursor:pointer}
button.p{background:var(--acc);color:#111;border-color:var(--acc);font-weight:700}button.d{color:var(--bad)}
button:active{transform:scale(.97)}
input,select,textarea{width:100%;background:#0e151e;color:var(--text);border:1px solid var(--line);border-radius:9px;padding:9px;font:inherit}
input[type=range]{padding:0;accent-color:var(--acc)}
ul{list-style:none;margin:0;padding:0}li{display:flex;align-items:center;gap:6px;padding:7px 0;border-bottom:1px solid var(--line)}
li .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}li.on .n{color:var(--teal);font-weight:700}
li button{padding:5px 9px}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--dim)}
label{display:block;font-size:13px;color:var(--dim);margin:8px 0 4px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--acc);color:#111;padding:9px 16px;border-radius:99px;font-weight:600;display:none}
</style></head><body>
<header><b>CX8ANM</b><span>Radio Internet &middot; GeekNest V5A</span><div class="chip" id="net">...</div></header>
<main>
<section id="now"><h2>Ahora suena</h2>
<div class="st" id="name">-</div><div class="ti" id="title"></div>
<div class="sm"><span class="dot" id="dot"></span><span id="state"></span> <span id="codec"></span></div>
<div class="row" style="margin-top:12px"><button onclick="api('step?d=-1')">&#9198;</button><button class="p" id="pp" onclick="playPause()">&#9654; Escuchar</button><button onclick="api('step?d=1')">&#9197;</button><button id="mute" onclick="toggleMute()">Silenciar</button></div>
<label>Volumen <span id="vtxt"></span></label><input type="range" min="0" max="21" id="vol" oninput="vtxt.textContent=this.value" onchange="api('vol?v='+this.value)">
</section>
<section><h2>Emisoras</h2><ul id="list"></ul>
<label>Nueva emisora</label><div class="grid"><input id="nn" placeholder="Nombre"><input id="nu" placeholder="http://... o https://..."></div>
<div class="row" style="margin-top:8px"><button class="p" onclick="addSt()">Agregar</button><button onclick="saveList()">Guardar lista</button><button onclick="toggleTxt()">Editar como texto</button></div>
<div id="txtbox" style="display:none;margin-top:8px"><textarea id="txt" rows="8" placeholder="Nombre|URL (una por línea)"></textarea>
<div class="row" style="margin-top:8px"><button class="p" onclick="saveTxt()">Guardar texto</button><button class="d" onclick="resetList()">Restaurar predeterminadas</button></div></div>
<p class="sm">Formatos: MP3 y AAC (http/https). Las listas .m3u/.pls se deben abrir y copiar la URL directa del stream.</p>
</section>
<section><h2>WiFi</h2>
<div class="row"><button onclick="scan()">Buscar redes</button><span class="sm" id="scanst"></span></div>
<label>Red</label><select id="ssids"><option value="">(elegir o escribir abajo)</option></select>
<input id="ssid" placeholder="Nombre de la red (SSID)" style="margin-top:6px">
<label>Clave</label><input id="pass" type="password" placeholder="Clave WiFi">
<div class="row" style="margin-top:10px"><button class="p" onclick="saveWifi()">Guardar y reiniciar</button></div>
</section>
<section><h2>Ajustes</h2>
<label>Zona horaria (también ajusta el reloj del modo RF)</label><select id="tz" onchange="api('tz?m='+this.value)"></select>
<label>Brillo de pantalla</label><input type="range" min="16" max="255" id="bri" onchange="api('bri?v='+this.value)">
<div class="row" style="margin-top:12px"><button onclick="if(confirm('Pasar al receptor RF (SI4732)?'))api('rf')">Pasar a receptor RF</button></div>
<p class="sm" id="sys"></p>
</section>
</main><div id="toast"></div>
<script>
let S={},L=[],busy=false;const $=id=>document.getElementById(id);
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e.t);e.t=setTimeout(()=>e.style.display='none',1800)}
async function api(p,body){const o={method:'POST'};if(body!==undefined){o.body=body;o.headers={'Content-Type':'text/plain'}}
 const r=await fetch('/api/'+p,o);const t=await r.text();if(!r.ok)toast(t||'Error');setTimeout(poll,300);return t}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function render(){const ul=$('list');ul.innerHTML='';L.forEach((s,i)=>{const li=document.createElement('li');if(i==S.idx)li.className='on';
 li.innerHTML='<button onclick="api(\'play?i='+i+'\')">&#9654;</button><span class="n" title="'+esc(s.u)+'">'+(i+1)+'. '+esc(s.n)+'</span>'+
 '<button onclick="mv('+i+',-1)">&#8593;</button><button onclick="mv('+i+',1)">&#8595;</button><button onclick="ed('+i+')">&#9998;</button><button class="d" onclick="del('+i+')">&#10005;</button>';ul.appendChild(li)})}
function mv(i,d){const j=i+d;if(j<0||j>=L.length)return;[L[i],L[j]]=[L[j],L[i]];render();toast('Recordá guardar la lista')}
function del(i){if(!confirm('Borrar '+L[i].n+'?'))return;L.splice(i,1);render();toast('Recordá guardar la lista')}
function ed(i){const n=prompt('Nombre',L[i].n);if(n===null)return;const u=prompt('URL',L[i].u);if(u===null)return;L[i]={n:n.trim(),u:u.trim()};render();toast('Recordá guardar la lista')}
function addSt(){const n=$('nn').value.trim(),u=$('nu').value.trim();if(!/^https?:\/\//.test(u)){toast('URL inválida');return}L.push({n:n||u,u});$('nn').value=$('nu').value='';render();saveList()}
function text(){return L.map(s=>s.n.replace(/\|/g,'/')+'|'+s.u).join('\n')}
async function saveList(){await api('stations',text());toast('Lista guardada');loadList()}
function toggleTxt(){const b=$('txtbox');b.style.display=b.style.display=='none'?'block':'none';$('txt').value=text()}
async function saveTxt(){await api('stations',$('txt').value);toast('Lista guardada');loadList()}
async function resetList(){if(!confirm('Volver a la lista original?'))return;await api('stations/reset');loadList()}
async function loadList(){const t=await (await fetch('/api/stations')).text();L=t.split('\n').filter(x=>x.trim()).map(x=>{const k=x.indexOf('|');return k<0?{n:x,u:x}:{n:x.slice(0,k),u:x.slice(k+1)}});render()}
function playPause(){api(S.state=='PLAYING'||S.state=='CONNECTING'?'stop':'play?i='+S.idx)}
function toggleMute(){api('mute?m='+(S.mute?0:1))}
async function scan(){$('scanst').textContent='buscando...';try{const n=await (await fetch('/api/scan')).json();const s=$('ssids');s.innerHTML='<option value="">(elegir)</option>';
 n.forEach(w=>{const o=document.createElement('option');o.value=w.s;o.textContent=w.s+'  ('+w.r+' dBm'+(w.e?'':', abierta')+')';s.appendChild(o)});$('scanst').textContent=n.length+' redes'}catch(e){$('scanst').textContent='error'}}
$('ssids').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};
async function saveWifi(){const s=$('ssid').value.trim();if(!s){toast('Falta la red');return}
 const f=new URLSearchParams();f.set('ssid',s);f.set('pass',$('pass').value);await fetch('/api/wifi',{method:'POST',body:f});toast('Guardado, reiniciando...')}
for(let m=-720;m<=840;m+=30){const o=document.createElement('option');o.value=m;const h=Math.trunc(Math.abs(m)/60),mm=Math.abs(m)%60;o.textContent='UTC'+(m<0?'-':'+')+h+(mm?':'+mm:'');$('tz').appendChild(o)}
async function poll(){if(busy)return;busy=true;try{const s=await (await fetch('/api/status')).json();const first=!S.ver;S=s;
 $('net').textContent=s.ap?'Modo configuración':(s.ssid+'  '+s.ip);$('name').textContent=s.name||'-';$('title').textContent=s.title||'';
 const st={PLAYING:['En el aire','var(--ok)'],CONNECTING:['Conectando...','var(--acc)'],ERROR:[s.err||'Error','var(--bad)'],IDLE:['Detenido','var(--dim)']}[s.state];
 $('state').textContent=st[0];$('dot').style.background=st[1];$('codec').textContent=s.state=='PLAYING'?(s.codec+' '+(s.br?Math.round(s.br/1000)+' kbps':'')):'';
 $('pp').innerHTML=(s.state=='PLAYING'||s.state=='CONNECTING')?'&#9632; Detener':'&#9654; Escuchar';$('mute').textContent=s.mute?'Activar sonido':'Silenciar';
 if(document.activeElement!=$('vol')){$('vol').value=s.vol;$('vtxt').textContent=s.vol}
 if(document.activeElement!=$('bri'))$('bri').value=s.bri;if(document.activeElement!=$('tz'))$('tz').value=s.tz;
 $('sys').textContent='Firmware '+s.ver+' - batería '+s.bat+' V - memoria libre '+Math.round(s.heap/1024)+' KB';
 if(first||s.count!=L.length)loadList();else render()}catch(e){$('net').textContent='sin conexión'}busy=false}
poll();setInterval(poll,2000);
</script></body></html>)HTML";

void send_text(int code, const String& text) { server.send(code, "text/plain; charset=utf-8", text); }
int arg_int(const char* name, int fallback) {
  return server.hasArg(name) ? server.arg(name).toInt() : fallback;
}

void handle_root() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}
void handle_not_found() {
  if (captive) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  } else send_text(404, "No encontrado");
}
void handle_scan() {
  const int n = WiFi.scanNetworks(false, false);
  String json = "[";
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    ssid.replace("\\", "\\\\"); ssid.replace("\"", "\\\"");
    if (json.length() > 1) json += ",";
    json += "{\"s\":\"" + ssid + "\",\"r\":" + String(WiFi.RSSI(i)) + ",\"e\":" +
            (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}
void handle_wifi() {
  const String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) { send_text(400, "Datos invalidos"); return; }
  net_config.ssid = ssid; net_config.password = pass;
  net_config_save_wifi();
  send_text(200, "ok");
  delay(300);
  net_action_wifi_saved();
}
void handle_stations_post() {
  const String body = server.arg("plain");
  net_station_t backup[NET_MAX_STATIONS];
  const int backup_count = net_config.count;
  for (int i = 0; i < backup_count; ++i) backup[i] = net_config.stations[i];
  if (!net_config_parse_stations(body)) {
    for (int i = 0; i < backup_count; ++i) net_config.stations[i] = backup[i];
    net_config.count = backup_count;
    send_text(400, "La lista no tiene ninguna URL valida");
    return;
  }
  net_config_save_stations();
  net_action_stations_changed();
  send_text(200, String(net_config.count));
}
} // namespace

void webui_begin(bool captive_portal) {
  captive = captive_portal;
  if (captive) dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handle_root);
  server.on("/api/status", HTTP_GET, [] { server.send(200, "application/json", net_status_json()); });
  server.on("/api/stations", HTTP_GET, [] { send_text(200, net_config_stations_text()); });
  server.on("/api/stations", HTTP_POST, handle_stations_post);
  server.on("/api/stations/reset", HTTP_POST, [] {
    net_config_reset_stations(); net_config_save_stations(); net_action_stations_changed(); send_text(200, "ok");
  });
  server.on("/api/play", HTTP_POST, [] { net_action_play(arg_int("i", 0)); send_text(200, "ok"); });
  server.on("/api/stop", HTTP_POST, [] { net_action_stop(); send_text(200, "ok"); });
  server.on("/api/step", HTTP_POST, [] { net_action_step(arg_int("d", 1)); send_text(200, "ok"); });
  server.on("/api/vol", HTTP_POST, [] { net_action_volume(arg_int("v", 10)); send_text(200, "ok"); });
  server.on("/api/mute", HTTP_POST, [] { net_action_mute(arg_int("m", 0) != 0); send_text(200, "ok"); });
  server.on("/api/bri", HTTP_POST, [] { net_action_brightness(arg_int("v", 160)); send_text(200, "ok"); });
  server.on("/api/tz", HTTP_POST, [] { net_action_timezone(arg_int("m", -180)); send_text(200, "ok"); });
  server.on("/api/scan", HTTP_GET, handle_scan);
  server.on("/api/wifi", HTTP_POST, handle_wifi);
  server.on("/api/rf", HTTP_POST, [] { send_text(200, "ok"); delay(300); net_action_switch_rf(); });
  server.onNotFound(handle_not_found);
  server.begin();
}

void webui_update(void) {
  if (captive) dns.processNextRequest();
  server.handleClient();
}
