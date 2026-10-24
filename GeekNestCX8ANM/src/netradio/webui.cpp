#include "webui.h"
#include "actions.h"
#include "config.h"
#include "updater.h"
#include "ui/splash.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
extern "C" {
#include "app/boot_mode.h"
#include "app/rf_remote.h"
}

namespace {
WebServer server(80);
DNSServer dns;
bool captive = false;
webui_mode_t mode = WEBUI_NET;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CX8ANM Radio</title>
<style>
:root{--bg:#0b0f14;--panel:#121a24;--line:#243040;--text:#e6e9ee;--dim:#8a93a0;--acc:#ffa31a;--teal:#2bb5f5;--ok:#36c26b;--bad:#f04a4a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:10px;padding:14px 16px;background:var(--panel);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:2}
header b{color:var(--acc);font-size:18px;letter-spacing:.5px}header span{color:var(--dim);font-size:13px}
.chip{margin-left:auto;font-size:12px;padding:3px 9px;border-radius:99px;border:1px solid var(--line);color:var(--dim);white-space:nowrap}
main{max-width:680px;margin:0 auto;padding:12px 16px 40px}
section{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px;margin:12px 0}
h2{margin:0 0 10px;font-size:12px;letter-spacing:1px;color:var(--dim);font-weight:600;text-transform:uppercase}
.st{font-size:22px;font-weight:700}.ti{min-height:22px;margin:6px 0}.sm{color:var(--dim);font-size:13px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button{background:#1b2633;color:var(--text);border:1px solid var(--line);border-radius:9px;padding:9px 13px;font:inherit;cursor:pointer}
button.p{background:var(--acc);color:#111;border-color:var(--acc);font-weight:700}button.d{color:var(--bad)}button.on{border-color:var(--teal);color:var(--teal);font-weight:700}
button:disabled{opacity:.35}button:active{transform:scale(.97)}
input,select,textarea{width:100%;background:#0e151e;color:var(--text);border:1px solid var(--line);border-radius:9px;padding:9px;font:inherit}
input[type=range]{padding:0;accent-color:var(--acc)}input[type=checkbox]{width:auto}
ul{list-style:none;margin:0;padding:0}li{display:flex;align-items:center;gap:6px;padding:7px 0;border-bottom:1px solid var(--line)}
li .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}li.on .n{color:var(--teal);font-weight:700}li button{padding:5px 9px}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--dim)}
label{display:block;font-size:13px;color:var(--dim);margin:8px 0 4px}label.ck{display:flex;gap:8px;align-items:center;color:var(--text)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.freq{font:700 44px/1.1 ui-monospace,Consolas,monospace;color:var(--acc);letter-spacing:1px}.unit{font-size:18px;color:var(--dim);margin-left:6px}
.meter{height:8px;background:#0e151e;border-radius:5px;overflow:hidden;flex:1}.meter i{display:block;height:100%;background:var(--teal)}
.banner{border-color:var(--acc)}.bar{height:10px;background:#0e151e;border-radius:6px;overflow:hidden;margin-top:8px}.bar i{display:block;height:100%;background:var(--acc);width:0}
.hide{display:none}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--acc);color:#111;padding:9px 16px;border-radius:99px;font-weight:600;display:none;z-index:5}
</style></head><body>
<header><b>CX8ANM</b><span id="sub">GeekNest V5A</span><div class="chip" id="net">...</div></header>
<main>
<section id="upd" class="banner hide"><h2>Actualización disponible</h2>
<div><b id="updv"></b> <span class="sm" id="updn"></span></div>
<div class="row" style="margin-top:10px"><button class="p" id="updb" onclick="startUpdate()">Actualizar ahora</button><span class="sm" id="upds"></span></div>
<div class="bar hide" id="updbar"><i id="updp"></i></div></section>

<section class="net hide"><h2>Ahora suena</h2>
<div class="st" id="name">-</div><div class="ti" id="title"></div>
<div class="sm"><span class="dot" id="dot"></span><span id="state"></span> <span id="codec"></span></div>
<div class="row" style="margin-top:12px"><button onclick="api('step?d=-1')">&#9198;</button><button class="p" id="pp" onclick="playPause()">&#9654; Escuchar</button><button onclick="api('step?d=1')">&#9197;</button><button id="mute" onclick="api('mute?m='+(S.mute?0:1))">Silenciar</button></div>
<label>Volumen <span id="vtxt"></span></label><input type="range" min="0" max="21" id="vol" oninput="vtxt.textContent=this.value" onchange="api('vol?v='+this.value)">
</section>
<section class="net hide"><h2>Emisoras</h2><ul id="list"></ul>
<label>Nueva emisora</label><div class="grid"><input id="nn" placeholder="Nombre"><input id="nu" placeholder="http://... o https://..."></div>
<div class="row" style="margin-top:8px"><button class="p" onclick="addSt()">Agregar</button><button onclick="saveList()">Guardar lista</button><button onclick="toggleTxt()">Editar como texto</button></div>
<div id="txtbox" class="hide" style="margin-top:8px"><textarea id="txt" rows="8" placeholder="Nombre|URL (una por línea)"></textarea>
<div class="row" style="margin-top:8px"><button class="p" onclick="saveTxt()">Guardar texto</button><button class="d" onclick="resetList()">Restaurar predeterminadas</button></div></div>
<p class="sm">Formatos: MP3 y AAC (http/https).</p></section>

<section class="rf hide"><h2>Receptor <span id="rband"></span> <span id="rmode"></span></h2>
<div><span class="freq" id="freq">---</span><span class="unit" id="funit"></span></div>
<div class="row sm" style="margin:6px 0"><span>Señal</span><div class="meter"><i id="rs"></i></div><span id="rsv"></span><span>SNR <b id="snr"></b></span><span id="stereo"></span></div>
<div class="ti"><b id="ps"></b> <span class="sm" id="rt"></span></div>
<div class="row"><button onclick="api('rf/seek?d=-1')">&#9194; Buscar</button><button onclick="api('rf/step?d=-1')">&#9664;</button><button onclick="api('rf/step?d=1')">&#9654;</button><button onclick="api('rf/seek?d=1')">Buscar &#9193;</button><button onclick="api('rf/stepsize?d=1')" id="stepb">Paso</button></div>
<label>Ir a frecuencia (MHz o kHz)</label><div class="row"><input id="fin" placeholder="ej: 100.3  -  7100  -  999" style="flex:1" onkeydown="if(event.key=='Enter')tuneTo()"><button class="p" onclick="tuneTo()">Sintonizar</button></div>
<label>Banda</label><div class="row" id="bands"></div>
<label>Modo</label><div class="row" id="modes"></div>
<label>Volumen <span id="rvt"></span></label><input type="range" min="0" max="63" id="rvol" oninput="rvt.textContent=this.value" onchange="api('rf/vol?v='+this.value)">
<div class="row" style="margin-top:8px"><button id="rmute" onclick="api('rf/mute?m='+(S.mute?0:1))">Silenciar</button><span class="sm" id="env"></span></div>
</section>
<section class="rf hide"><h2>Favoritos</h2><ul id="favs"></ul>
<div class="row" style="margin-top:8px"><button class="p" onclick="addFav()">Guardar frecuencia actual</button></div></section>

<section><h2>WiFi</h2>
<div class="row"><button onclick="scan()">Buscar redes</button><span class="sm" id="scanst"></span></div>
<label>Red</label><select id="ssids"><option value="">(elegir o escribir abajo)</option></select>
<input id="ssid" placeholder="Nombre de la red (SSID)" style="margin-top:6px">
<label>Clave</label><input id="pass" type="password" placeholder="Clave WiFi">
<div class="row" style="margin-top:10px"><button class="p" onclick="saveWifi()">Guardar y reiniciar</button></div></section>

<section><h2>Ajustes</h2>
<label>Zona horaria (reloj de los dos modos)</label><select id="tz" onchange="api('tz?m='+this.value)"></select>
<div class="net hide"><label>Brillo de pantalla</label><input type="range" min="16" max="255" id="bri" onchange="api('bri?v='+this.value)"></div>
<label class="ck"><input type="checkbox" id="rfwifi" onchange="api('options?rfwifi='+(this.checked?1:0))"> WiFi y control web también en modo receptor RF</label>
<p class="sm">Apagarlo reduce el ruido del WiFi cuando escuchás onda corta débil.</p>
<label class="ck"><input type="checkbox" id="autoupd" onchange="api('options?autoupd='+(this.checked?1:0))"> Buscar actualizaciones automáticamente</label>
<div class="row" style="margin-top:12px"><button id="swb" onclick="switchMode()">Cambiar de modo</button></div></section>

<section><h2>Firmware</h2>
<div class="sm" id="fw"></div>
<div class="row" style="margin-top:8px"><button onclick="checkUpdate()">Buscar actualización en GitHub</button><span class="sm" id="chk"></span></div>
<label>Instalar un archivo (CX8ANM-...-app.bin)</label><div class="row"><input type="file" id="bin" accept=".bin" style="flex:1"><button onclick="upload()">Subir e instalar</button></div>
<div class="bar hide" id="upbar"><i id="upp"></i></div>
<p class="sm" id="sys"></p></section>
</main><div id="toast"></div>
<script>
let S={},L=[],F=[],busy=false,lastMode='';const $=id=>document.getElementById(id);
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e.t);e.t=setTimeout(()=>e.style.display='none',2200)}
async function api(p,body){const o={method:'POST'};if(body!==undefined){o.body=body;o.headers={'Content-Type':'text/plain'}}
 try{const r=await fetch('/api/'+p,o);const t=await r.text();if(!r.ok)toast(t||'Error');setTimeout(poll,300);return t}catch(e){toast('Sin conexión con la radio')}}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function show(cls,on){document.querySelectorAll('.'+cls).forEach(e=>e.classList.toggle('hide',!on))}
/* ---- Internet radio ---- */
function render(){const ul=$('list');ul.innerHTML='';L.forEach((s,i)=>{const li=document.createElement('li');if(i==S.idx)li.className='on';
 li.innerHTML='<button onclick="api(\'play?i='+i+'\')">&#9654;</button><span class="n" title="'+esc(s.u)+'">'+(i+1)+'. '+esc(s.n)+'</span>'+
 '<button onclick="mv('+i+',-1)">&#8593;</button><button onclick="mv('+i+',1)">&#8595;</button><button onclick="ed('+i+')">&#9998;</button><button class="d" onclick="del('+i+')">&#10005;</button>';ul.appendChild(li)})}
function mv(i,d){const j=i+d;if(j<0||j>=L.length)return;[L[i],L[j]]=[L[j],L[i]];render();toast('Recordá guardar la lista')}
function del(i){if(!confirm('¿Borrar '+L[i].n+'?'))return;L.splice(i,1);render();toast('Recordá guardar la lista')}
function ed(i){const n=prompt('Nombre',L[i].n);if(n===null)return;const u=prompt('URL',L[i].u);if(u===null)return;L[i]={n:n.trim(),u:u.trim()};render();toast('Recordá guardar la lista')}
function addSt(){const n=$('nn').value.trim(),u=$('nu').value.trim();if(!/^https?:\/\//.test(u)){toast('URL inválida');return}L.push({n:n||u,u});$('nn').value=$('nu').value='';render();saveList()}
function text(){return L.map(s=>s.n.replace(/\|/g,'/')+'|'+s.u).join('\n')}
async function saveList(){await api('stations',text());toast('Lista guardada');loadList()}
function toggleTxt(){$('txtbox').classList.toggle('hide');$('txt').value=text()}
async function saveTxt(){await api('stations',$('txt').value);toast('Lista guardada');loadList()}
async function resetList(){if(!confirm('¿Volver a la lista original?'))return;await api('stations/reset');loadList()}
async function loadList(){const t=await (await fetch('/api/stations')).text();L=t.split('\n').filter(x=>x.trim()).map(x=>{const k=x.indexOf('|');return k<0?{n:x,u:x}:{n:x.slice(0,k),u:x.slice(k+1)}});render()}
function playPause(){api(S.state=='PLAYING'||S.state=='CONNECTING'?'stop':'play?i='+S.idx)}
/* ---- Receiver ---- */
const MODES=['FM','AM','LSB','USB'];
function fmtFreq(hz,band){if(hz>=30e6)return[(hz/1e6).toFixed(band=='AIR'?3:2),'MHz'];const k=hz/1e3;return[(S.rmode_index>=2?k.toFixed(2):k.toFixed(0)),'kHz']}
function inBand(hz){return (S.bands||[]).some(b=>b.a&&hz>=b.lo&&hz<=b.hi)}
function tuneTo(){const v=parseFloat($('fin').value.replace(',','.'));if(!(v>0)){toast('Escribí una frecuencia');return}
 const c=[v<300?v*1e6:v*1e3,v<300?v*1e3:v*1e6].map(Math.round).find(inBand);if(!c){toast('Fuera de las bandas del receptor');return}api('rf/tune?hz='+c)}
function renderRF(){const[f,u]=fmtFreq(S.hz,S.band);$('freq').textContent=f;$('funit').textContent=u;$('rband').textContent=S.band;$('rmode').textContent=S.rmode;
 $('rs').style.width=Math.min(100,S.rssi*100/70)+'%';$('rsv').textContent=S.rssi+' dBµV';$('snr').textContent=S.snr+' dB';$('stereo').textContent=S.stereo?'ESTÉREO':'';
 $('ps').textContent=S.ps||'';$('rt').textContent=S.rt||'';$('stepb').textContent='Paso '+(S.step>=1000?S.step/1000+' kHz':S.step+' Hz');
 const b=$('bands');if(b.childElementCount!=S.bands.length){b.innerHTML='';S.bands.forEach((x,i)=>{const e=document.createElement('button');e.textContent=x.n;e.onclick=()=>api('rf/band?i='+i);b.appendChild(e)})}
 [...b.children].forEach((e,i)=>{e.className=i==S.band_index?'on':'';e.disabled=!S.bands[i].a});
 const m=$('modes');if(!m.childElementCount)MODES.forEach((x,i)=>{const e=document.createElement('button');e.textContent=x;e.onclick=()=>api('rf/mode?m='+i);m.appendChild(e)});
 const fm=S.band=='VHF',air=S.band=='AIR';[...m.children].forEach((e,i)=>{e.className=i==S.rmode_index?'on':'';e.disabled=fm?i!=0:(i==0||(air&&i>1))});
 if(document.activeElement!=$('rvol')){$('rvol').value=S.vol;$('rvt').textContent=S.vol}$('rmute').textContent=S.mute?'Activar sonido':'Silenciar';
 $('env').textContent=(S.temp!==undefined?S.temp+' °C  '+S.hum+' %  ':'')+(S.err?'Error: '+S.err:'')}
function renderFavs(){const ul=$('favs');ul.innerHTML='';if(!F.length)ul.innerHTML='<li class="sm">Todavía no hay favoritos</li>';
 F.forEach((x,i)=>{const li=document.createElement('li');const[f,u]=fmtFreq(x.hz,'');li.innerHTML='<button onclick="tuneFav('+i+')">&#9654;</button><span class="n">'+esc(x.n)+' <span class="sm">'+f+' '+u+' '+x.m+'</span></span><button class="d" onclick="delFav('+i+')">&#10005;</button>';ul.appendChild(li)})}
async function loadFavs(){const t=await (await fetch('/api/rf/favs')).text();F=t.split('\n').filter(x=>x.trim()).map(x=>{const p=x.split('|');return{n:p[0],hz:+p[1],m:p[2]||'AM'}}).filter(x=>x.hz>0);renderFavs()}
async function saveFavs(){await api('rf/favs',F.map(x=>x.n.replace(/\|/g,'/')+'|'+x.hz+'|'+x.m).join('\n'));renderFavs()}
function addFav(){const n=prompt('Nombre del favorito',S.ps||(fmtFreq(S.hz,S.band).join(' ')));if(n===null)return;F.push({n:n.trim()||'Favorito',hz:S.hz,m:S.rmode});saveFavs();toast('Favorito guardado')}
function delFav(i){if(!confirm('¿Borrar '+F[i].n+'?'))return;F.splice(i,1);saveFavs()}
async function tuneFav(i){const x=F[i];await api('rf/tune?hz='+x.hz);const k=MODES.indexOf(x.m);if(k>=0)setTimeout(()=>api('rf/mode?m='+k),400)}
/* ---- Common ---- */
async function scan(){$('scanst').textContent='buscando...';try{const n=await (await fetch('/api/scan')).json();const s=$('ssids');s.innerHTML='<option value="">(elegir)</option>';
 n.forEach(w=>{const o=document.createElement('option');o.value=w.s;o.textContent=w.s+'  ('+w.r+' dBm'+(w.e?'':', abierta')+')';s.appendChild(o)});$('scanst').textContent=n.length+' redes'}catch(e){$('scanst').textContent='error'}}
$('ssids').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};
async function saveWifi(){const s=$('ssid').value.trim();if(!s){toast('Falta la red');return}
 const f=new URLSearchParams();f.set('ssid',s);f.set('pass',$('pass').value);await fetch('/api/wifi',{method:'POST',body:f});toast('Guardado, reiniciando...')}
for(let m=-720;m<=840;m+=30){const o=document.createElement('option');o.value=m;const h=Math.trunc(Math.abs(m)/60),mm=Math.abs(m)%60;o.textContent='UTC'+(m<0?'-':'+')+h+(mm?':'+mm:'');$('tz').appendChild(o)}
function switchMode(){const to=S.mode=='rf'?'net':'rf';if(confirm(to=='rf'?'¿Pasar al receptor RF?':'¿Pasar a Radio Internet?'))api('mode?to='+to)}
async function checkUpdate(){$('chk').textContent='consultando GitHub...';await api('update/check');let n=0;const t=setInterval(async()=>{await poll();if(!S.update.checking||++n>20){clearInterval(t);
 $('chk').textContent=S.update.error?S.update.error:(S.update.available?'Hay una versión nueva: '+S.update.latest:'Ya tenés la última versión ('+S.update.current+')')}},1500)}
function startUpdate(){if(!confirm('¿Instalar la versión '+S.update.latest+'? La radio se reinicia sola al terminar.'))return;api('update/start');$('upds').textContent='Descargando... no apagues la radio';$('updbar').classList.remove('hide')}
function upload(){const f=$('bin').files[0];if(!f){toast('Elegí el archivo .bin');return}if(!/app\.bin$/i.test(f.name)&&!confirm('El archivo no termina en -app.bin. ¿Seguro?'))return;
 const x=new XMLHttpRequest();const fd=new FormData();fd.append('firmware',f,f.name);$('upbar').classList.remove('hide');
 x.upload.onprogress=e=>{if(e.lengthComputable)$('upp').style.width=(e.loaded*100/e.total)+'%'};
 x.onload=()=>{toast(x.status==200?'Instalado, reiniciando...':(x.responseText||'Error'));if(x.status!=200)$('upbar').classList.add('hide')};
 x.onerror=()=>toast('Se cortó la conexión');x.open('POST','/api/update/upload');x.send(fd)}
async function poll(){if(busy)return;busy=true;try{const s=await (await fetch('/api/status')).json();S=s;
 if(s.mode!=lastMode){lastMode=s.mode;show('net',s.mode=='net');show('rf',s.mode=='rf');$('sub').textContent=s.mode=='rf'?'Receptor RF':'Radio Internet';
  $('swb').textContent=s.mode=='rf'?'Pasar a Radio Internet':'Pasar a receptor RF';if(s.mode=='net')loadList();else loadFavs()}
 $('net').textContent=s.ap?'Modo configuración':(s.ssid+'  '+s.ip);
 if(s.mode=='net'){$('name').textContent=s.name||'-';$('title').textContent=s.title||'';
  const st={PLAYING:['En el aire','var(--ok)'],CONNECTING:['Conectando...','var(--acc)'],ERROR:[s.err||'Error','var(--bad)'],IDLE:['Detenido','var(--dim)']}[s.state];
  $('state').textContent=st[0];$('dot').style.background=st[1];$('codec').textContent=s.state=='PLAYING'?(s.codec+' '+(s.br?Math.round(s.br/1000)+' kbps':'')):'';
  $('pp').innerHTML=(s.state=='PLAYING'||s.state=='CONNECTING')?'&#9632; Detener':'&#9654; Escuchar';$('mute').textContent=s.mute?'Activar sonido':'Silenciar';
  if(document.activeElement!=$('vol')){$('vol').value=s.vol;$('vtxt').textContent=s.vol}if(document.activeElement!=$('bri'))$('bri').value=s.bri;
  if(s.count!=L.length)loadList();else render()} else renderRF();
 if(document.activeElement!=$('tz'))$('tz').value=s.tz;$('rfwifi').checked=s.rfwifi;$('autoupd').checked=s.autoupd;
 const u=s.update;$('upd').classList.toggle('hide',!(u.available||u.installing));$('updv').textContent='Versión '+u.latest;$('updn').textContent=u.notes||'';
 if(u.installing){$('updbar').classList.remove('hide');$('updp').style.width=Math.max(0,u.progress)+'%';$('upds').textContent='Instalando '+Math.max(0,u.progress)+'%...';$('updb').disabled=true}
 else if(u.error&&u.available)$('upds').textContent=u.error;
 $('fw').textContent='Versión instalada: '+u.current+(u.checked?(u.available?' - nueva: '+u.latest:' - al día'):'');
 $('sys').textContent='Batería '+s.bat+' V - memoria libre '+Math.round(s.heap/1024)+' KB'}catch(e){$('net').textContent='sin conexión'}busy=false}
poll();setInterval(poll,2000);
</script></body></html>)HTML";

void send_text(int code, const String& text) { server.send(code, "text/plain; charset=utf-8", text); }
int arg_int(const char* name, int fallback) { return server.hasArg(name) ? server.arg(name).toInt() : fallback; }
bool is_net() { return mode == WEBUI_NET; }
bool require(bool net) {
  if (is_net() == net) return true;
  send_text(409, net ? "Disponible en modo Radio Internet" : "Disponible en modo receptor RF");
  return false;
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
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) { send_text(400, "Datos inválidos"); return; }
  net_config.ssid = ssid; net_config.password = pass;
  net_config_save_wifi();
  send_text(200, "ok");
  delay(300);
  if (is_net()) net_action_wifi_saved();
  cx8_message("WiFi guardado", "Reiniciando...");
  delay(300);
  boot_mode_restart();
}
void handle_stations_post() {
  if (!require(true)) return;
  const String body = server.arg("plain");
  net_station_t backup[NET_MAX_STATIONS];
  const int backup_count = net_config.count;
  for (int i = 0; i < backup_count; ++i) backup[i] = net_config.stations[i];
  if (!net_config_parse_stations(body)) {
    for (int i = 0; i < backup_count; ++i) net_config.stations[i] = backup[i];
    net_config.count = backup_count;
    send_text(400, "La lista no tiene ninguna URL válida");
    return;
  }
  net_config_save_stations();
  net_action_stations_changed();
  send_text(200, String(net_config.count));
}
void handle_timezone() {
  const int m = arg_int("m", -180);
  if (m < -720 || m > 840 || m % 30) { send_text(400, "Zona inválida"); return; }
  if (is_net()) net_action_timezone(m);
  else {
    net_config.utc_offset = (int16_t)m;
    net_config_save_state();
    rf_remote_set_utc_offset((int16_t)m);
  }
  send_text(200, "ok");
}
void handle_options() {
  if (server.hasArg("rfwifi")) net_config.rf_wifi = arg_int("rfwifi", 1) != 0;
  if (server.hasArg("autoupd")) net_config.auto_update_check = arg_int("autoupd", 1) != 0;
  net_config_save_state();
  send_text(200, !is_net() && !net_config.rf_wifi ? "Se apagará el WiFi del modo RF al reiniciar" : "ok");
}
void handle_mode() {
  const String to = server.arg("to");
  send_text(200, "ok");
  delay(300);
  if (to == "rf" && is_net()) net_action_switch_rf();
  else if (to == "net" && !is_net()) rf_remote_switch_to_net();
}
void handle_update_check() {
  if (!updater_check_async()) { send_text(updater.checking ? 200 : 503, updater.checking ? "ok" : updater.error); return; }
  send_text(200, "ok");
}
void handle_update_start() {
  if (!updater.available) { send_text(400, "Primero buscá una actualización"); return; }
  send_text(200, "ok");
  delay(300);
  if (is_net()) net_action_update_start();
  else { // The receiver's frame buffers leave too little RAM for TLS: install from internet mode.
    updater_set_pending(true);
    rf_remote_switch_to_net();
  }
}
String upload_error;
void handle_upload_done() {
  if (upload_error.length()) { send_text(400, upload_error); return; }
  send_text(200, "ok");
  delay(500);
  cx8_message("Firmware instalado", "Reiniciando...");
  delay(300);
  boot_mode_restart();
}
void handle_upload_data() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    upload_error = "";
    if (is_net()) net_action_stop();
    cx8_message("Instalando firmware", "Recibiendo archivo por WiFi...");
    updater_upload_begin();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    updater_upload_write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (!updater_upload_end(&upload_error)) cx8_message("Error al instalar", upload_error.c_str());
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    updater_upload_end(nullptr);
    upload_error = "Se interrumpió la subida";
  }
}
} // namespace

void webui_begin(bool captive_portal, webui_mode_t web_mode) {
  captive = captive_portal;
  mode = web_mode;
  if (captive) dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handle_root);
  server.on("/api/status", HTTP_GET, [] { server.send(200, "application/json", is_net() ? net_status_json() : rf_status_json()); });
  // Internet radio
  server.on("/api/stations", HTTP_GET, [] { send_text(200, net_config_stations_text()); });
  server.on("/api/stations", HTTP_POST, handle_stations_post);
  server.on("/api/stations/reset", HTTP_POST, [] {
    if (!require(true)) return;
    net_config_reset_stations(); net_config_save_stations(); net_action_stations_changed(); send_text(200, "ok");
  });
  server.on("/api/play", HTTP_POST, [] { if (require(true)) { net_action_play(arg_int("i", 0)); send_text(200, "ok"); } });
  server.on("/api/stop", HTTP_POST, [] { if (require(true)) { net_action_stop(); send_text(200, "ok"); } });
  server.on("/api/step", HTTP_POST, [] { if (require(true)) { net_action_step(arg_int("d", 1)); send_text(200, "ok"); } });
  server.on("/api/vol", HTTP_POST, [] { if (require(true)) { net_action_volume(arg_int("v", 10)); send_text(200, "ok"); } });
  server.on("/api/mute", HTTP_POST, [] { if (require(true)) { net_action_mute(arg_int("m", 0) != 0); send_text(200, "ok"); } });
  server.on("/api/bri", HTTP_POST, [] { if (require(true)) { net_action_brightness(arg_int("v", 160)); send_text(200, "ok"); } });
  // Receiver
  server.on("/api/rf/tune", HTTP_POST, [] {
    if (!require(false)) return;
    if (rf_remote_tune_hz((uint32_t)server.arg("hz").toInt())) send_text(200, "ok"); else send_text(400, "Frecuencia fuera de banda");
  });
  server.on("/api/rf/step", HTTP_POST, [] { if (require(false)) { rf_remote_step(arg_int("d", 1)); send_text(200, "ok"); } });
  server.on("/api/rf/seek", HTTP_POST, [] { if (require(false)) { rf_remote_seek(arg_int("d", 1)); send_text(200, "ok"); } });
  server.on("/api/rf/band", HTTP_POST, [] { if (require(false)) { rf_remote_band(arg_int("i", 3)); send_text(200, "ok"); } });
  server.on("/api/rf/mode", HTTP_POST, [] {
    if (!require(false)) return;
    if (rf_remote_mode(arg_int("m", 0))) send_text(200, "ok"); else send_text(400, "Modo no disponible en esta banda");
  });
  server.on("/api/rf/stepsize", HTTP_POST, [] { if (require(false)) { rf_remote_step_size(arg_int("d", 1)); send_text(200, "ok"); } });
  server.on("/api/rf/vol", HTTP_POST, [] { if (require(false)) { rf_remote_volume(arg_int("v", 18)); send_text(200, "ok"); } });
  server.on("/api/rf/mute", HTTP_POST, [] { if (require(false)) { rf_remote_mute(arg_int("m", 0) != 0); send_text(200, "ok"); } });
  server.on("/api/rf/favs", HTTP_GET, [] { send_text(200, net_config.favorites); });
  server.on("/api/rf/favs", HTTP_POST, [] {
    net_config.favorites = server.arg("plain");
    net_config_save_favorites();
    send_text(200, "ok");
  });
  // Common
  server.on("/api/tz", HTTP_POST, handle_timezone);
  server.on("/api/options", HTTP_POST, handle_options);
  server.on("/api/mode", HTTP_POST, handle_mode);
  server.on("/api/scan", HTTP_GET, handle_scan);
  server.on("/api/wifi", HTTP_POST, handle_wifi);
  server.on("/api/update/check", HTTP_POST, handle_update_check);
  server.on("/api/update/start", HTTP_POST, handle_update_start);
  server.on("/api/update/upload", HTTP_POST, handle_upload_done, handle_upload_data);
  server.onNotFound(handle_not_found);
  server.begin();
}

void webui_update(void) {
  if (captive) dns.processNextRequest();
  server.handleClient();
}
