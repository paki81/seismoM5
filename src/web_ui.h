#ifndef _web_ui_H_
#define _web_ui_H_

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SeismoM5</title>
<link rel="icon" href="data:,">
<style>
:root{--bg:#0f1419;--card:#18202a;--line:#273241;--fg:#e6edf3;--mut:#8b98a8;--acc:#3b9eff;--ok:#2fbf71;--warn:#f0a020;--bad:#f04848;--r:12px}
@media (prefers-color-scheme:light){:root{--bg:#f3f5f8;--card:#fff;--line:#dde3ea;--fg:#17202a;--mut:#5d6b7b}}
*{box-sizing:border-box}
body{margin:0;font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--fg)}
header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:12px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line)}
header h1{font-size:18px;margin:0;flex:1}
header small{color:var(--mut);font-weight:400}
.pill{padding:4px 10px;border-radius:99px;font-size:12px;font-weight:600;letter-spacing:.04em;background:var(--line);color:var(--fg)}
.pill.ok{background:var(--ok);color:#fff}.pill.warn{background:var(--warn);color:#111}.pill.bad{background:var(--bad);color:#fff;animation:pulse 1s infinite}
@keyframes pulse{50%{opacity:.55}}
nav{display:flex;gap:4px;overflow-x:auto;padding:8px 12px;background:var(--bg)}
nav button{flex:0 0 auto;background:none;border:0;color:var(--mut);padding:8px 12px;border-radius:8px;font:inherit;cursor:pointer}
nav button.on{background:var(--card);color:var(--fg);box-shadow:0 0 0 1px var(--line)}
main{max-width:860px;margin:0 auto;padding:8px 12px 40px}
section{display:none}section.on{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:16px;margin:12px 0}
.card h2{font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.kpi{background:var(--bg);border-radius:10px;padding:10px 12px}
.kpi b{display:block;font-size:22px;font-variant-numeric:tabular-nums}
.kpi span{color:var(--mut);font-size:12px}
.big b{font-size:34px}
canvas{width:100%;height:180px;display:block;background:var(--bg);border-radius:10px}
.legend{display:flex;gap:14px;font-size:12px;color:var(--mut);margin-top:6px}
.legend i{display:inline-block;width:10px;height:3px;margin-right:5px;vertical-align:middle}
label.f{display:block;margin:0 0 12px}
label.f>span{display:block;font-size:13px;color:var(--mut);margin-bottom:4px}
input[type=text],input[type=number],input[type=password],select{width:100%;padding:9px 10px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg);font:inherit}
input:focus,select:focus{outline:2px solid var(--acc);outline-offset:-1px}
input:read-only{opacity:.55}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media (max-width:560px){.row{grid-template-columns:1fr}}
.sw{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid var(--line)}
.sw:first-of-type{border-top:0}
.sw small{display:block;color:var(--mut)}
.tg{position:relative;width:44px;height:24px;flex:0 0 auto}
.tg input{opacity:0;width:0;height:0}
.tg i{position:absolute;inset:0;background:var(--line);border-radius:99px;transition:.2s;cursor:pointer}
.tg i:before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.tg input:checked+i{background:var(--acc)}.tg input:checked+i:before{transform:translateX(20px)}
.tg input:disabled+i{opacity:.4}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.btn{border:0;border-radius:8px;padding:10px 16px;font:inherit;font-weight:600;cursor:pointer;background:var(--acc);color:#fff}
.btn.sec{background:var(--line);color:var(--fg)}.btn.bad{background:var(--bad)}
.btn:disabled{opacity:.5;cursor:default}
.hint{font-size:12px;color:var(--mut);margin:4px 0 0}
pre{margin:0;background:#0a0d11;color:#c9d6e3;border-radius:10px;padding:10px;height:380px;overflow:auto;font:12px/1.4 ui-monospace,Menlo,Consolas,monospace;white-space:pre-wrap}
code{background:var(--bg);padding:1px 5px;border-radius:5px;font-size:12px}
table{width:100%;border-collapse:collapse;font-size:13px}td{padding:6px 0;border-top:1px solid var(--line)}td:last-child{text-align:right;color:var(--mut);word-break:break-all}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(80px);background:var(--fg);color:var(--bg);padding:10px 16px;border-radius:10px;transition:.25s;font-weight:600}
#toast.on{transform:translateX(-50%) translateY(0)}
.banner{display:none;background:var(--warn);color:#111;padding:8px 16px;font-size:13px;text-align:center}
</style>
</head>
<body>
<header><h1 id="title">SeismoM5 <small id="host"></small></h1><span class="pill" id="mq">MQTT</span><span class="pill" id="st">…</span></header>
<div class="banner" id="apb">Setup access point active: configure WiFi in the <b>Network</b> tab.</div>
<nav id="nav">
<button data-t="dash" class="on">Dashboard</button><button data-t="det">Detection</button><button data-t="mqtt">MQTT / HA</button><button data-t="net">Network</button><button data-t="log">Log</button><button data-t="sys">System</button>
</nav>
<main>
<section id="dash" class="on">
<div class="card"><div class="grid">
<div class="kpi big"><span>PGA (g)</span><b id="k_pga">–</b></div>
<div class="kpi big"><span id="k_trl">Trigger (g)</span><b id="k_tr">–</b></div>
<div class="kpi"><span>Method</span><b id="k_m">–</b></div>
<div class="kpi"><span>STA / LTA</span><b id="k_sl">–</b></div>
</div></div>
<div class="card"><h2>Live</h2><canvas id="cv"></canvas>
<div class="legend" id="lg"></div></div>
<div class="card"><div class="grid">
<div class="kpi"><span>MQTT</span><b id="k_mq">–</b></div>
<div class="kpi"><span>WiFi</span><b id="k_rs">–</b></div>
<div class="kpi"><span>Uptime</span><b id="k_up">–</b></div>
<div class="kpi"><span>Role</span><b id="k_role">–</b></div>
</div></div>
</section>

<section id="det">
<form id="f_det">
<div class="card"><h2>Trigger</h2>
<div class="sw"><div>STA/LTA method<small>Use the STA/LTA ratio instead of a fixed PGA threshold</small></div><label class="tg"><input type="checkbox" name="sta_lta"><i></i></label></div>
<label class="f"><span>PGA trigger (g)</span><input type="number" name="pga_trigger" step="0.0001" min="0.0001" max="2"></label>
<label class="f"><span>Event update period (s)</span><input type="number" name="update_period" min="1" max="3600"><p class="hint">Periodic <code>event</code> message when there is no earthquake.</p></label>
</div>
<div class="card"><h2>Device</h2>
<label class="f"><span>LCD standby brightness (7–15)</span><input type="number" name="lcd_brightness" min="7" max="15"></label>
<div class="sw"><div>Speaker (SPK HAT)<small>Alarm tone during an earthquake</small></div><label class="tg"><input type="checkbox" name="speaker_enable"><i></i></label></div>
<div class="sw"><div>Continuous graph<small>Draw the graph also when there is no earthquake</small></div><label class="tg"><input type="checkbox" name="continuous_graph"><i></i></label></div>
<div class="sw"><div>Master<small>Confirm earthquakes with the slave stick</small></div><label class="tg"><input type="checkbox" name="master"><i></i></label></div>
<label class="f" style="margin-top:12px"><span>Logging</span><select name="logging"><option value="0">None</option><option value="1">Serial only</option><option value="2">Web log only</option><option value="3">Serial + Web log</option></select></label>
</div>
<div class="btns"><button class="btn">Save</button></div>
</form>
</section>

<section id="mqtt">
<form id="f_mqtt">
<div class="card"><h2>Broker</h2>
<div class="sw"><div>MQTT enabled</div><label class="tg"><input type="checkbox" name="mqtt_enabled"><i></i></label></div>
<div class="row"><label class="f"><span>Host</span><input type="text" name="mqtt_host" placeholder="192.168.1.10"></label>
<label class="f"><span>Port</span><input type="number" name="mqtt_port" min="1" max="65535"></label></div>
<div class="row"><label class="f"><span>Username</span><input type="text" name="mqtt_user" autocomplete="off"></label>
<label class="f"><span>Password</span><input type="password" name="mqtt_pass" autocomplete="new-password" placeholder="unchanged"></label></div>
<div class="row"><label class="f"><span>Base topic</span><input type="text" name="mqtt_base"></label>
<label class="f"><span>Slave state topic</span><input type="text" name="mqtt_slave_topic"></label></div>
<p class="hint">Topics: <code id="tp"></code></p>
</div>
<div class="card"><h2>Home Assistant</h2>
<div class="sw"><div>MQTT Discovery<small>Creates the device and all entities automatically. Remove the manual YAML sensors to avoid duplicates.</small></div><label class="tg"><input type="checkbox" name="ha_discovery"><i></i></label></div>
<label class="f"><span>Discovery prefix</span><input type="text" name="ha_prefix"></label>
</div>
<div class="btns"><button class="btn">Save &amp; reconnect</button><button type="button" class="btn sec" data-a="discovery">Republish discovery</button><button type="button" class="btn sec" data-a="update">Publish event</button></div>
</form>
</section>

<section id="net">
<form id="f_net">
<div class="card"><h2>WiFi</h2>
<label class="f"><span>SSID</span><input type="text" name="wifi_ssid"></label>
<label class="f"><span>Password</span><input type="password" name="wifi_pass" autocomplete="new-password" placeholder="unchanged"></label>
<p class="hint">Changing WiFi restarts the device. If it cannot connect, the access point <code id="apn"></code> (password <code>seismom5</code>) is started.</p>
</div>
<div class="card"><h2>Web interface login</h2>
<div class="row"><label class="f"><span>Username</span><input type="text" name="web_user" autocomplete="off" placeholder="disabled"></label>
<label class="f"><span>Password</span><input type="password" name="web_pass" autocomplete="new-password" placeholder="unchanged"></label></div>
<p class="hint">Leave the username empty to disable authentication.</p>
</div>
<div class="btns"><button class="btn">Save</button></div>
</form>
</section>

<section id="log">
<div class="card"><h2>Log</h2><pre id="lo"></pre>
<div class="btns"><button class="btn sec" id="lc">Clear</button><label class="sw" style="border:0;padding:0;gap:8px">Autoscroll<label class="tg"><input type="checkbox" id="las" checked><i></i></label></label></div>
<p class="hint">Requires logging set to “Web log” or “Serial + Web log”.</p></div>
</section>

<section id="sys">
<div class="card"><h2>Actions</h2>
<div class="btns"><button class="btn sec" data-a="recalibrate" data-c="Recalibrate the MPU? Keep the device still.">Recalibrate MPU</button><button class="btn sec" data-a="mqtt_toggle">Toggle MQTT</button><button class="btn bad" data-a="restart" data-c="Restart SeismoM5?">Restart</button></div></div>
<div class="card"><h2>Info</h2><table id="info"></table></div>
</section>
</main>
<div id="toast"></div>
<script>
const $=s=>document.querySelector(s),$$=s=>document.querySelectorAll(s);
let S={},C={},hist=[],N=240;
function toast(t){const e=$('#toast');e.textContent=t;e.classList.add('on');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('on'),2500)}
function tab(t){$$('nav button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));$$('section').forEach(s=>s.classList.toggle('on',s.id==t));location.hash=t;if(t=='dash')draw()}
$$('nav button').forEach(b=>b.onclick=()=>tab(b.dataset.t));
if(location.hash.length>1&&$('#'+location.hash.slice(1)))tab(location.hash.slice(1));
async function api(u,body){const r=await fetch(u,body?{method:'POST',body:new URLSearchParams(body)}:{});if(!r.ok)throw new Error(r.status);return r.json()}
function fill(f,o){for(const e of f.elements){if(!e.name||!(e.name in o))continue;if(e.type=='checkbox')e.checked=!!o[e.name];else if(e.type!='password')e.value=o[e.name]}}
function collect(f){const o={};for(const e of f.elements){if(!e.name||e.disabled)continue;if(e.type=='checkbox')o[e.name]=e.checked?1:0;else if(e.type=='password'){if(e.value)o[e.name]=e.value}else o[e.name]=e.value}return o}
function upd(){const r=$('[name=pga_trigger]');r.readOnly=$('[name=sta_lta]').checked;const b=$('[name=mqtt_base]').value||'seismoM5';$('#tp').textContent=['state','event','status','pga_trigger','settings','diag','command'].map(x=>b+'/'+x).join('  ')}
async function loadCfg(){C=await api('/api/settings');['#f_det','#f_mqtt','#f_net'].forEach(f=>fill($(f),C));$('[name=master]').disabled=C.slave;$('#apn').textContent=C.ap_ssid;
$('[name=mqtt_pass]').placeholder=C.has_mqtt_pass?'unchanged':'none';$('[name=web_pass]').placeholder=C.has_web_pass?'unchanged':'none';upd()}
$('[name=sta_lta]').onchange=upd;$('[name=mqtt_base]').oninput=upd;
for(const id of ['#f_det','#f_mqtt','#f_net'])$(id).onsubmit=async e=>{e.preventDefault();const f=e.target,o=collect(f);
if(id=='#f_net'&&!confirm('Save network settings? The device will restart if WiFi changes.'))return;
try{const r=await api('/api/settings',o);toast(r.restart?'Saved – restarting…':'Saved');f.querySelectorAll('[type=password]').forEach(p=>p.value='');loadCfg()}catch(x){toast('Error '+x.message)}};
$$('[data-a]').forEach(b=>b.onclick=async()=>{if(b.dataset.c&&!confirm(b.dataset.c))return;try{await api('/api/action',{do:b.dataset.a});toast('OK')}catch(x){toast('Error '+x.message)}});
function fmtUp(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return d?d+'d '+h+'h':h?h+'h '+m+'m':m+'m '+s%60+'s'}
function stat(s){S=s;const st=$('#st');st.textContent=s.state;st.className='pill '+(s.state=='EARTHQUAKE'?'bad':s.state=='LISTENING'?'ok':'warn');
$('#mq').className='pill '+(s.mqtt.connected?'ok':s.mqtt.enabled?'warn':'');$('#host').textContent=s.hostname;
$('#k_pga').textContent=s.pga.toFixed(4);$('#k_tr').textContent=s.sta_lta?'S/LTA':s.pga_trigger.toFixed(4);$('#k_m').textContent=s.sta_lta?'STA/LTA':'PGA';
$('#k_sl').textContent=s.sta.toFixed(4)+' / '+s.lta.toFixed(4);$('#k_mq').textContent=s.mqtt.connected?'Connected':s.mqtt.enabled?'Connecting':'Off';
$('#k_rs').textContent=s.wifi.ap?'AP mode':s.wifi.rssi+' dBm';$('#k_up').textContent=fmtUp(s.uptime);$('#k_role').textContent=s.slave?'Slave':s.master?'Master'+(s.slave_eq?' (slave EQ)':''):'Standalone';
$('#apb').style.display=s.wifi.ap?'block':'none';
hist.push([s.pga,s.sta,s.lta,s.state=='EARTHQUAKE']);if(hist.length>N)hist.shift();if($('#dash').classList.contains('on'))draw();
$('#info').innerHTML=[['Firmware',s.version],['Model',s.model],['Hostname',s.hostname],['IP',s.wifi.ip],['SSID',s.wifi.ssid],['MAC',s.mac],['MQTT broker',s.mqtt.host||'–'],['MQTT reconnects',s.mqtt.reconnects],['Free heap',(s.heap/1024).toFixed(1)+' kB']].map(r=>'<tr><td>'+r[0]+'</td><td>'+r[1]+'</td></tr>').join('')}
function draw(){const c=$('#cv'),x=c.getContext('2d'),w=c.width=c.clientWidth*devicePixelRatio,h=c.height=c.clientHeight*devicePixelRatio;x.clearRect(0,0,w,h);if(!hist.length)return;
const sl=S.sta_lta,ser=sl?[[1,'#f04848','STA'],[2,'#3b9eff','LTA']]:[[0,'#2fbf71','PGA']];let mx=sl?0.002:S.pga_trigger*1.3;
hist.forEach(p=>ser.forEach(s=>mx=Math.max(mx,p[s[0]])));const Y=v=>h-6-(v/mx)*(h-12),dx=w/(N-1),o=N-hist.length;
x.fillStyle='rgba(240,72,72,.15)';hist.forEach((p,i)=>{if(p[3])x.fillRect((o+i)*dx-dx/2,0,dx,h)});
if(!sl){x.strokeStyle='#f0a020';x.setLineDash([6,6]);x.beginPath();x.moveTo(0,Y(S.pga_trigger));x.lineTo(w,Y(S.pga_trigger));x.stroke();x.setLineDash([])}
x.lineWidth=2*devicePixelRatio;ser.forEach(s=>{x.strokeStyle=s[1];x.beginPath();hist.forEach((p,i)=>{const X=(o+i)*dx;i?x.lineTo(X,Y(p[s[0]])):x.moveTo(X,Y(p[s[0]]))});x.stroke()});
$('#lg').innerHTML=ser.map(s=>'<span><i style="background:'+s[1]+'"></i>'+s[2]+'</span>').join('')+(sl?'':'<span><i style="background:#f0a020"></i>Trigger</span>')+'<span>max '+mx.toFixed(4)+' g</span>'}
const lo=$('#lo');function addLog(t){lo.textContent+=t+'\n';if(lo.textContent.length>60000)lo.textContent=lo.textContent.slice(-40000);if($('#las').checked)lo.scrollTop=lo.scrollHeight}
$('#lc').onclick=()=>lo.textContent='';
function sse(){const es=new EventSource('/events');es.addEventListener('status',e=>stat(JSON.parse(e.data)));es.addEventListener('log',e=>addLog(e.data));es.onerror=()=>{$('#st').textContent='OFFLINE';$('#st').className='pill'}}
api('/api/status').then(stat).catch(()=>{});loadCfg().catch(()=>toast('Cannot load settings'));sse();window.onresize=draw;
</script>
</body>
</html>
)rawliteral";

#endif
