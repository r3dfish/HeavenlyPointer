// ============================================================================
//  web_assets.h  -  The phone control UI, embedded in flash (PROGMEM) and
//  served from "/" by WebControl. Vanilla HTML/CSS/JS, no framework.
// ============================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>HeavenlyPointer</title>
<style>
:root{--bg:#0c1016;--card:#161c26;--mut:#7b8aa0;--acc:#37c7e6;--ok:#3ad07a;--warn:#ff6b6b}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:#eef2f7}
h1{font-size:18px;margin:0;padding:14px 16px;background:#10151d;border-bottom:1px solid #222c3a}
.wrap{max-width:560px;margin:0 auto;padding:12px}
.card{background:var(--card);border:1px solid #222c3a;border-radius:12px;padding:14px;margin:12px 0}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 10px}
.big{font-size:26px;font-weight:700}.acc{color:var(--acc)}.ok{color:var(--ok)}.warn{color:var(--warn)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 14px}
.kv{display:flex;justify-content:space-between;border-bottom:1px solid #1d2533;padding:4px 0;font-size:14px}
.kv span:first-child{color:var(--mut)}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
select,input{width:100%;padding:10px;border-radius:8px;border:1px solid #2a3647;background:#0f141c;color:#fff;font-size:15px}
.row{display:flex;gap:10px}.row>*{flex:1}
button{width:100%;padding:12px;margin-top:12px;border:0;border-radius:8px;background:var(--acc);color:#04222c;font-weight:700;font-size:15px;cursor:pointer;transition:background .12s,transform .05s,box-shadow .12s}
button:hover{background:#5fd6ef}
button:active{background:#1f9fbd;transform:translateY(1px);box-shadow:inset 0 2px 6px rgba(0,0,0,.35)}
button.sec{background:#27313f;color:#cfd9e6}
button.sec:hover{background:#36445c}
button.sec:active{background:#1b2330;transform:translateY(1px);box-shadow:inset 0 2px 6px rgba(0,0,0,.4)}
.btns{display:flex;gap:10px}.btns button{margin-top:0}
small{color:var(--mut)}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;vertical-align:middle}
</style></head><body>
<h1>🛰 HeavenlyPointer</h1>
<div class=wrap>

  <div class=card>
    <h2>Now tracking</h2>
    <div class=big id=name>—</div>
    <div id=vis><span class=dot id=visdot></span><small id=vistext>—</small></div>
    <div class=grid style=margin-top:10px>
      <div class=kv><span>Azimuth</span><span id=az>—</span></div>
      <div class=kv><span>Elevation</span><span id=el>—</span></div>
      <div class=kv><span>Range</span><span id=rng>—</span></div>
      <div class=kv><span>Altitude</span><span id=alt>—</span></div>
      <div class=kv><span>Velocity</span><span id=vel>—</span></div>
      <div class=kv><span>Sub-point</span><span id=sub>—</span></div>
      <div class=kv><span>Head pan/tilt</span><span id=servo>—</span></div>
      <div class=kv><span>Sats loaded</span><span id=sats>—</span></div>
    </div>
    <div class=btns style=margin-top:12px>
      <button class=sec onclick=act('prev')>◀ Prev</button>
      <button class=sec onclick=act('next')>Next ▶</button>
    </div>
    <div style=margin-top:8px><small id=foot>—</small></div>
  </div>

  <div class=card>
    <h2>Satellite configuration</h2>
    <label>Catalog group</label>
    <select id=group>
      <option value=visual>visual — brightest naked-eye sats</option>
      <option value=stations>stations — ISS &amp; space stations</option>
      <option value=amateur>amateur — ham radio satellites</option>
      <option value=active>active — all active (large)</option>
      <option value=weather>weather — NOAA/weather sats</option>
      <option value=starlink>starlink — Starlink fleet</option>
      <option value=gps-ops>gps-ops — GPS satellites</option>
    </select>
    <small>Changing the group re-downloads the catalog (takes a few seconds).</small>

    <label>Name filter (optional, e.g. "ISS" or "NOAA")</label>
    <input id=filter placeholder="blank = track everything">

    <label>Orbit class — which satellites move across the sky</label>
    <select id=orbit>
      <option value=2>LEO only — low orbit, fast passes (recommended)</option>
      <option value=1>Skip geostationary — LEO + MEO</option>
      <option value=0>All orbits</option>
    </select>

    <label>Reach limits — only tracks satellites the head can point at</label>
    <div class=row>
      <div><label>Min elev °</label><input id=minel type=number min=0 max=85 step=1></div>
      <div><label>Max elev °</label><input id=maxel type=number min=5 max=90 step=1></div>
      <div><label>Pan ±°</label><input id=panlim type=number min=10 max=120 step=5></div>
    </div>
    <small>A satellite leaving these limits auto-switches to the next one in range.</small>

    <label>Clock UTC offset (hours) — display only</label>
    <input id=tzoff type=number step=0.25 placeholder="auto-detected from IP">
    <small>e.g. -5 (US Eastern), +1 (CET), +5.5 (India). Auto-locate refills this.</small>

    <label>Facing — which way the head points "forward"</label>
    <select id=facing>
      <option>NORTH</option><option>EAST</option><option>SOUTH</option><option>WEST</option>
    </select>

    <label>Observer location (decimal degrees)</label>
    <div id=geostat style="font-size:13px;margin:2px 0 6px;color:var(--mut)">—</div>
    <div class=row>
      <input id=lat type=number step=any placeholder=Latitude>
      <input id=lon type=number step=any placeholder=Longitude>
    </div>
    <button class=sec onclick=relocate()>Auto-locate from IP</button>

    <label style="display:flex;align-items:center;gap:8px;color:#eef2f7;margin-top:14px">
      <input type=checkbox id=leds style="width:auto"> LED bars (green = tracking, red = waiting)
    </label>

    <label style="display:flex;align-items:center;gap:8px;color:#eef2f7;margin-top:14px">
      <input type=checkbox id=sleepsched style="width:auto"> Sleep schedule (screen/LEDs/head off during window)
    </label>
    <div class=row style=margin-top:6px>
      <div><label>Sleep at</label><input id=sleepstart type=time></div>
      <div><label>Wake at</label><input id=sleepend type=time></div>
    </div>
    <small>Local time. Head parks, screen and LEDs go off; WiFi stays up.</small>

    <button onclick=saveCfg()>Save &amp; apply</button>
  </div>

  <div class=card>
    <h2>Actions</h2>
    <div class=btns>
      <button class=sec onclick=act('refetch')>Re-download TLEs</button>
      <button class=sec onclick=act('park')>Recenter head</button>
      <button class=sec onclick=act('recover')>Recover servos</button>
    </div>
    <button id=sleepbtn onclick=toggleSleep() style=margin-top:10px>SLEEP NOW</button>
  </div>

  <div class=card>
    <h2>Motor test</h2>
    <label style="display:flex;align-items:center;gap:8px;color:#eef2f7">
      <input type=checkbox id=testmode style="width:auto" onchange=setTest()> Test mode — pauses tracking, drive motors by hand
    </label>
    <div id=testpanel style=display:none>
      <label>Pan / azimuth motor: <b id=panv>0</b>°</label>
      <input type=range id=pan min=-120 max=120 step=5 value=0 oninput="document.getElementById('panv').textContent=this.value" onchange=sendTest()>
      <label>Tilt / elevation motor: <b id=tiltv>45</b>°</label>
      <input type=range id=tilt min=5 max=85 step=5 value=45 oninput="document.getElementById('tiltv').textContent=this.value" onchange=sendTest()>
      <div class=btns style=margin-top:8px>
        <button class=sec onclick="preset(0,45)">Center</button>
        <button class=sec onclick=sweepPan()>Sweep pan</button>
        <button class=sec onclick=sweepTilt()>Sweep tilt</button>
      </div>
      <div id=testread style="font-size:13px;margin-top:8px;color:var(--mut)">—</div>
      <small>Watch "actual": if it doesn't follow the command when you move pan, the azimuth servo isn't responding.</small>
    </div>
  </div>

  <div style="text-align:center;color:var(--mut);font-size:12px;margin:18px 0 10px">
    <a href="https://github.com/r3dfish/HeavenlyPointer" target=_blank rel=noopener style="color:var(--acc)">HeavenlyPointer on GitHub</a>
    &nbsp;·&nbsp;<span id=ver></span>
  </div>
</div>

<script>
const $=id=>document.getElementById(id);
const f1=(n,d=1)=>(n==null?'—':Number(n).toFixed(d));
const KM2MI=0.621371;
async function refresh(){
  try{
    const s=await (await fetch('/status.json')).json();
    $('sleepbtn').textContent = s.sleeping ? 'WAKE NOW' : 'SLEEP NOW';
    $('testmode').checked = s.testMode; $('testpanel').style.display = s.testMode?'block':'none';
    const gs=$('geostat');
    if(s.geoStatus===1){ gs.style.color='var(--ok)';
      gs.textContent='📍 '+(s.geoCity?s.geoCity+' ':'')+'('+f1(s.lat,3)+', '+f1(s.lon,3)+') — located via IP ✓'; }
    else if(s.geoStatus===2){ gs.style.color='var(--warn)';
      gs.textContent='⚠ IP location lookup failed — enter lat/lon below'; }
    else { gs.style.color='var(--mut)'; gs.textContent='location not yet determined'; }
    if(s.testMode){
      $('name').textContent='🔧 Motor test'; $('name').className='big';
      $('visdot').style.background='#864'; $('vistext').textContent='tracking paused';
      ['az','el','rng','alt','vel','sub','servo'].forEach(k=>$(k).textContent='—');
      const poff=s.panAct<-100, toff=s.tiltAct<-100;
      const pok=!poff&&Math.abs(s.panCmd-s.panAct)<=8, tok=!toff&&Math.abs(s.tiltCmd-s.tiltAct)<=8;
      const pa=poff?'NO RESPONSE':Math.round(s.panAct)+'°', ta=toff?'NO RESPONSE':Math.round(s.tiltAct)+'°';
      $('testread').innerHTML='actual — pan: <b style="color:'+(pok?'var(--ok)':'var(--warn)')+'">'+pa+'</b> (cmd '+Math.round(s.panCmd)+'°)　tilt: <b style="color:'+(tok?'var(--ok)':'var(--warn)')+'">'+ta+'</b> (cmd '+Math.round(s.tiltCmd)+'°)';
    }else if(s.sleeping){
      $('name').textContent='😴 Sleeping'; $('name').className='big';
      $('visdot').style.background='#446'; $('vistext').textContent='press WAKE NOW to resume';
      ['az','el','rng','alt','vel','sub','servo'].forEach(k=>$(k).textContent='—');
    }else if(s.tracking){
      $('name').textContent=s.name; $('name').className='big acc';
      $('az').textContent=f1(s.az)+'°'; $('el').textContent=f1(s.el)+'°';
      $('rng').textContent=Math.round(s.rangeKm*KM2MI)+' mi';
      $('alt').textContent=Math.round(s.altKm*KM2MI)+' mi';
      $('vel').textContent=f1(s.velKms,2)+' km/s';
      $('sub').textContent=f1(s.subLat,1)+', '+f1(s.subLon,1);
      $('servo').textContent=Math.round(s.pan)+'° / '+Math.round(s.tilt)+'°'+(s.inRange?'':' ⚠');
      $('visdot').style.background=s.vis>0?'var(--ok)':(s.vis==0?'#446':'#864');
      $('vistext').textContent=s.visText;
    }else{
      if(s.hasNext){
        const t=s.nextInSec, hh=Math.floor(t/3600), mm=Math.floor(t/60)%60, ss=t%60, p=n=>String(n).padStart(2,'0');
        $('name').textContent='⏱ '+(hh>0?`${hh}:${p(mm)}:${p(ss)}`:`${mm}:${p(ss)}`);
        $('name').className='big acc'; $('vistext').textContent='next: '+s.nextName;
      }else{
        $('name').textContent='Scanning the sky…'; $('name').className='big';
        $('vistext').textContent='waiting for a pass';
      }
      ['az','el','rng','alt','vel','sub','servo'].forEach(k=>$(k).textContent='—');
      $('visdot').style.background='#446';
    }
    $('sats').textContent=s.satCount;
    $('foot').textContent= s.testMode ? ('motor test · '+s.ip)
      : s.sleeping ? ('asleep · '+s.iso+' · '+s.ip)
      : ('group: '+s.group+'  ·  '+s.iso+'  ·  '+s.ip+'  ·  TLEs '+s.tleAgeMin+' min old');
  }catch(e){ $('foot').textContent='disconnected'; }
}
function toggleSleep(){ act($('sleepbtn').textContent.trim()==='SLEEP NOW'?'sleep':'wake'); }
function setTest(){ const on=$('testmode').checked; $('testpanel').style.display=on?'block':'none'; act(on?'test':'testexit'); }
function sendTest(){ const b=new URLSearchParams(); b.append('pan',$('pan').value); b.append('tilt',$('tilt').value);
  fetch('/test',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}); }
function preset(p,t){ $('pan').value=p; $('tilt').value=t; $('panv').textContent=p; $('tiltv').textContent=t; sendTest(); }
function sweepPan(){ preset(-120,45); setTimeout(()=>preset(120,45),3000); setTimeout(()=>preset(0,45),6000); }
function sweepTilt(){ preset(0,5); setTimeout(()=>preset(0,85),3000); setTimeout(()=>preset(0,45),6000); }
async function loadCfg(){
  const c=await (await fetch('/config.json')).json();
  $('group').value=c.group; $('filter').value=c.filter||'';
  $('minel').value=c.minElevation; $('maxel').value=c.maxElevation; $('panlim').value=c.panLimit;
  $('facing').value=c.facing;
  $('tzoff').value=c.tzOffsetHours; $('orbit').value=c.orbitClass; $('leds').checked=c.ledBars;
  $('sleepsched').checked=c.sleepSchedule; $('sleepstart').value=c.sleepStart; $('sleepend').value=c.sleepEnd;
  if(c.hasLocation){ $('lat').value=c.lat; $('lon').value=c.lon; }
  $('ver').textContent=c.version||'';
}
async function saveCfg(){
  const b=new URLSearchParams();
  ['group','filter','minel','maxel','panlim','facing','lat','lon','tzoff','orbit','sleepstart','sleepend'].forEach(k=>b.append(k,$(k).value));
  b.append('leds', $('leds').checked?'1':'0');
  b.append('sleepsched', $('sleepsched').checked?'1':'0');
  await fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});
  setTimeout(()=>{loadCfg();refresh();},400);
}
async function act(cmd){
  await fetch('/action',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'cmd='+cmd});
  setTimeout(refresh,400);
}
async function relocate(){ await act('relocate'); setTimeout(loadCfg,1500); }
loadCfg(); refresh(); setInterval(refresh,1500);
</script>
</body></html>)HTML";
