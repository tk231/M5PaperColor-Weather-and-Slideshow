// index_html.h - the page served at http://192.168.4.1/
// One self-contained file (no external scripts, fonts or images). The only outside
// requests it ever makes are to Open-Meteo (city search / weather preview), and those
// go through the phone's own mobile connection, never through the device.
//
// Tabs: Device (clock, battery) | Weather (location, interval) | Wi-Fi (home network) |
//       LED | Tests (diagnostics)
#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"PCHTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>PaperColor</title>
<style>
:root{--bg:#f2f2f5;--card:#fff;--fg:#111;--mut:#6b6b73;--line:#dcdce2;--acc:#6440ff;--ok:#2b8a3e;--bad:#c92a2a;--warn:#e67700}
@media (prefers-color-scheme:dark){:root{--bg:#101012;--card:#1c1c1f;--fg:#f2f2f4;--mut:#9a9aa3;--line:#34343a}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif}
.top{position:sticky;top:0;z-index:5;background:#000}
header{color:#fff;padding:12px 16px 8px;display:flex;align-items:center;justify-content:space-between}
header h1{margin:0;font-size:19px}
.badges{display:flex;gap:6px;align-items:center}
.chip{font-size:12px;padding:3px 10px;border-radius:99px;background:#333;color:#fff}
.chip.ok{background:var(--ok)} .chip.bad{background:var(--bad)} .chip.warn{background:var(--warn)}
nav{display:flex;overflow-x:auto;-webkit-overflow-scrolling:touch;scrollbar-width:none;padding:0 6px}
nav::-webkit-scrollbar{display:none}
nav button{flex:1 0 auto;background:none;border:0;border-bottom:3px solid transparent;color:#aaa;font:inherit;font-size:15px;padding:9px 14px;border-radius:0;cursor:pointer}
nav button.on{color:#fff;border-bottom-color:var(--acc)}
main{max-width:520px;margin:0 auto;padding:14px}
.tab{display:none} .tab.on{display:block}
.help p{margin:8px 0} .help ul,.help ol{margin:8px 0;padding-left:22px} .help li{margin:6px 0}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px}
.sec{font-size:12px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:22px 0 8px}
.sec.first{margin-top:0}
.big{font-size:22px;font-weight:600;font-variant-numeric:tabular-nums;word-break:break-word}
.sub{font-size:14px;margin-top:2px}
.kv{display:flex;justify-content:space-between;gap:12px;padding:7px 0;border-top:1px solid var(--line);font-size:15px}
.kv span{color:var(--mut);white-space:nowrap} .kv b{text-align:right;font-weight:500;font-variant-numeric:tabular-nums}
.note{color:var(--mut);font-size:13px;margin:8px 0 0}
.msg{font-size:14px;min-height:1.2em;margin-top:8px}
.ok{color:var(--ok)} .bad{color:var(--bad)} .na{color:var(--mut)}
label{display:block;margin:12px 0 4px;color:var(--mut);font-size:13px}
.inp{width:100%;font:inherit;padding:11px 12px;border:1px solid var(--line);border-radius:10px;background:var(--card);color:var(--fg)}
select.inp{appearance:none;-webkit-appearance:none}
.line{display:flex;gap:8px} .line .inp{flex:1;min-width:0}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button.btn{font:inherit;border:0;border-radius:10px;padding:11px 16px;background:var(--acc);color:#fff;cursor:pointer}
button.btn.alt{background:var(--line);color:var(--fg)}
button.btn:disabled{opacity:.5}
.btns{display:flex;gap:10px;margin-top:14px} .btns .btn{flex:1}
.list{margin-top:8px;border:1px solid var(--line);border-radius:10px;overflow:hidden}
.list:empty{display:none}
.item{display:flex;align-items:center;gap:10px;padding:10px 12px;border-top:1px solid var(--line);cursor:pointer}
.item:first-child{border-top:0}
.item.sel{background:rgba(100,64,255,.14)}
.item .nm{flex:1;min-width:0;word-break:break-word}
.item small{color:var(--mut);white-space:nowrap}
.sig{display:inline-flex;align-items:flex-end;gap:1px;height:14px}
.sig i{display:block;width:4px;background:var(--line)}
.sig i:nth-child(1){height:5px}.sig i:nth-child(2){height:8px}.sig i:nth-child(3){height:11px}.sig i:nth-child(4){height:14px}
.s1 i:nth-child(-n+1),.s2 i:nth-child(-n+2),.s3 i:nth-child(-n+3),.s4 i:nth-child(-n+4){background:var(--fg)}
.sw{display:flex;gap:8px;margin-top:10px}
.sw button{flex:1;height:36px;padding:0;border:1px solid var(--line);border-radius:10px;cursor:pointer}
input[type=color]{width:100%;height:46px;border:1px solid var(--line);border-radius:10px;background:var(--card);padding:3px}
input[type=range]{width:100%}
details{margin-top:12px} summary{color:var(--acc);cursor:pointer;font-size:15px}
.selbox{margin-top:12px;padding:10px 12px;border-radius:10px;background:rgba(100,64,255,.12);font-size:14px}
ul{list-style:none;margin:10px 0 0;padding:0}
li{padding:7px 0;border-top:1px solid var(--line);font-size:14px;word-break:break-word}
li:first-child{border-top:0} li b{display:inline-block;min-width:1.4em}
pre{background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:10px;font-size:12px;white-space:pre-wrap;word-break:break-word;margin:12px 0 0}
.add{margin:4px 0 0}
.filebtn{display:inline-block;text-align:center;cursor:pointer}
.edframe{margin:6px 0 10px}
#edCanvas{display:block;width:100%;height:auto;touch-action:none;border:1px solid var(--line);border-radius:8px;background:#fff;cursor:grab}
.edrow{display:flex;flex-wrap:wrap;gap:8px;margin:8px 0;align-items:center}
.seg{font:inherit;font-size:14px;padding:7px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--fg);cursor:pointer}
.seg.on{background:var(--acc);border-color:var(--acc);color:#fff}
.add select{font:inherit;font-size:14px;padding:7px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--fg)}
.add input[type=range]{width:100%}
.rng{display:flex;justify-content:space-between;margin:0 0 2px}
.chk{display:flex;align-items:center;gap:8px;color:var(--fg);font-size:14px;margin-top:12px}
.chk input{width:20px;height:20px}
.slbox{border:1px solid var(--line);border-radius:12px;padding:10px 12px;margin:10px 0}
progress{display:block;width:100%;height:10px;margin-top:12px}
.gal{margin-top:6px}
.gi{display:flex;gap:12px;padding:12px 0;border-top:1px solid var(--line)}
.gi:first-child{border-top:0}
.gi.off .th{opacity:.35}
.th{width:112px;height:75px;object-fit:contain;background:#fff;border:1px solid var(--line);border-radius:8px;flex:none;cursor:pointer}
.gm{flex:1;min-width:0}
.gn{font-weight:600;word-break:break-word}
.gd{font-size:13px;color:var(--mut);margin-top:2px}
.tag{display:inline-block;font-size:11px;padding:1px 8px;border-radius:99px;background:var(--line);color:var(--fg);margin-left:6px}
.tag.ok{background:var(--ok);color:#fff} .tag.bad{background:var(--bad);color:#fff}
.ga{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.ga button{font:inherit;font-size:13px;padding:6px 10px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--fg);cursor:pointer}
.ga button.on{background:var(--ok);border-color:var(--ok);color:#fff}
.ga button.del{color:var(--bad)}
.ga button:disabled{opacity:.35}
a{color:var(--acc)}
.tick.warn{background:var(--bad)}
.popcard .two{display:flex;gap:10px} .popcard .two .btn{flex:1;width:auto}
button.btn.bad{background:var(--bad)}
.popcard.wide{max-width:460px}
.popcard img{max-width:100%;border-radius:8px;border:1px solid var(--line);background:#fff}
.pop{position:fixed;inset:0;z-index:20;display:flex;align-items:center;justify-content:center;padding:20px;background:rgba(0,0,0,.5)}
.pop[hidden]{display:none}
.popcard{background:var(--card);color:var(--fg);border-radius:18px;padding:24px 20px 18px;width:100%;max-width:330px;text-align:center;box-shadow:0 10px 40px rgba(0,0,0,.35)}
.tick{width:54px;height:54px;line-height:54px;margin:0 auto 10px;border-radius:50%;background:var(--ok);color:#fff;font-size:30px;font-weight:700}
.popcard h3{margin:0 0 8px;font-size:20px}
.popcard p{margin:0 0 18px;color:var(--mut);font-size:15px;white-space:pre-line;word-break:break-word}
.popcard .btn{width:100%}
#banner{background:var(--bad);color:#fff;border-radius:10px;padding:10px 14px;margin-bottom:14px;font-size:14px}
footer{max-width:520px;margin:0 auto;padding:0 14px 28px;color:var(--mut);font-size:13px;text-align:center}
</style>
</head>
<body>
<div class="top">
  <header><h1>PaperColor</h1><span class="badges"><span id="prof" class="chip warn" hidden></span><span id="conn" class="chip">connecting&hellip;</span></span></header>
  <nav id="tabs">
    <button data-tab="device" class="on">Device</button>
    <button data-tab="weather">Weather</button>
    <button data-tab="upload">Upload Image</button>
    <button data-tab="gallery">Image Gallery</button>
    <button data-tab="wifi">Wi-Fi</button>
    <button data-tab="led">LED</button>
    <button data-tab="instructions">Instructions</button>
    <button data-tab="tests">Tests</button>
  </nav>
</div>

<main>
  <div id="banner" hidden>Lost connection to the device. Its Wi-Fi may have closed (idle timeout). Rejoin the Wi-Fi and reload this page.</div>

  <!-- ===== Device ===== -->
  <section id="tab-device" class="tab on"><div class="card">
    <h2 class="sec first">Device clock</h2>
    <div class="big" id="devTime">&ndash;</div>
    <div class="sub" id="syncState">&nbsp;</div>
    <div class="kv"><span>This phone</span><b id="phoneTime">&ndash;</b></div>
    <div class="kv"><span>Time zone</span><b id="tz">&ndash;</b></div>
    <div class="btns"><button id="syncBtn" class="btn">Sync clock now</button></div>
    <p class="note" id="syncMsg">The clock is synced automatically when this page opens.</p>
    <h2 class="sec">Battery</h2>
    <div class="kv" style="border-top:0"><span>Charge</span><b id="bat">&ndash;</b></div>
    <h2 class="sec">microSD card</h2>
    <div class="kv" style="border-top:0"><span>Status</span><b id="sdState" class="wrap">&ndash;</b></div>
  </div></section>

  <!-- ===== Weather ===== -->
  <section id="tab-weather" class="tab"><div class="card">
    <h2 class="sec first">Location</h2>
    <div class="big" id="locNow">Not set</div>
    <div class="line" style="margin-top:12px">
      <input id="q" class="inp" type="search" placeholder="Search for a city" autocomplete="off" autocapitalize="words" enterkeyhint="search">
      <button id="qBtn" class="btn" style="flex:none">Search</button>
    </div>
    <div id="qMsg" class="msg"></div>
    <div id="qList" class="list"></div>
    <details id="manual">
      <summary>Enter coordinates instead</summary>
      <div class="two">
        <div><label for="mLat">Latitude</label><input id="mLat" class="inp" inputmode="decimal" placeholder="52.37" autocomplete="off"></div>
        <div><label for="mLon">Longitude</label><input id="mLon" class="inp" inputmode="decimal" placeholder="4.89" autocomplete="off"></div>
      </div>
      <label for="mName">Name shown on the screen</label>
      <input id="mName" class="inp" maxlength="36" placeholder="Home" autocomplete="off">
      <div class="btns"><button id="mUse" class="btn alt">Use these coordinates</button></div>
      <div id="mMsg" class="msg"></div>
    </details>
    <div id="selBox" class="selbox" hidden>Selected: <b id="selName"></b> <span id="selCoords"></span><br>Not saved yet.</div>

    <h2 class="sec">Weather update</h2>
    <label for="fetchSel">Fetch every</label>
    <select id="fetchSel" class="inp"></select>

    <div class="btns"><button id="wxSave" class="btn">Save</button><button id="wxPrev" class="btn alt">Preview</button></div>
    <div id="wxMsg" class="msg"></div>
    <p class="note">City search and the preview use this phone's mobile data. Typing coordinates works without it.</p>
  </div></section>

  <!-- ===== Wi-Fi ===== -->
  <!-- ===== Images ===== -->
  <section id="tab-upload" class="tab"><div class="card">
    <h2 class="sec first">Upload Image</h2>
    <div id="addBox" class="add">
      <div id="edPick">
        <p class="note">Choose a photo (JPEG, PNG or TIFF) and adjust it here before it goes to the frame. The photo itself stays on the card, so you can edit it again later.</p>
        <label class="btn alt filebtn" for="edFile">Choose a photo</label>
        <input id="edFile" type="file" accept="image/jpeg,image/png,image/tiff,.jpg,.jpeg,.png,.tif,.tiff" hidden>
      </div>
      <div id="edBox" hidden>
        <div class="gd" id="edInfo"></div>
        <div class="edframe"><canvas id="edCanvas" width="600" height="400"></canvas></div>
        <div class="edrow">
          <button id="edLand" type="button" class="seg on">Landscape</button><button id="edPort" type="button" class="seg">Portrait</button>
          <button id="edRot" type="button" class="seg">Rotate</button>
        </div>
        <div class="edrow">
          <button id="edFill" type="button" class="seg on">Fill</button><button id="edFit" type="button" class="seg">Fit</button>
          <select id="edBorder" aria-label="Border colour" hidden><option value="white">White border</option><option value="black">Black border</option></select>
        </div>
        <label for="edZoom">Zoom <span id="edZoomV">100%</span></label>
        <input id="edZoom" type="range" min="100" max="800" step="1" value="100">
        <label for="edB">Brightness <span id="edBV">0</span></label>
        <input id="edB" type="range" min="-100" max="100" step="1" value="0">
        <label for="edC">Contrast <span id="edCV">0</span></label>
        <input id="edC" type="range" min="-100" max="100" step="1" value="0">
        <label for="edS">Saturation <span id="edSV">0</span></label>
        <input id="edS" type="range" min="-100" max="100" step="1" value="0">
        <label for="edW">Warmth <span id="edWV">0</span></label>
        <input id="edW" type="range" min="-100" max="100" step="1" value="0">
        <div class="gd rng"><span>cooler</span><span>warmer</span></div>
        <label for="edT">Tint <span id="edTV">0</span></label>
        <input id="edT" type="range" min="-100" max="100" step="1" value="0">
        <div class="gd rng"><span>greener</span><span>pinker</span></div>
        <label for="edDither">Colours</label>
        <select id="edDither"><option value="fs">Floyd&ndash;Steinberg (default)</option><option value="panel">Panel (as the factory page)</option><option value="nearest">Nearest colour</option></select>
        <label for="edName">Name</label>
        <input id="edName" class="inp" type="text" maxlength="40" autocapitalize="off" autocomplete="off" spellcheck="false">
        <div class="btns"><button id="edSave" type="button" class="btn">Save</button><button id="edReset" type="button" class="btn alt">Reset</button><button id="edCancel" type="button" class="btn alt">Cancel</button></div>
        <progress id="edProg" max="100" value="0" hidden></progress>
      </div>
      <div id="edMsg" class="msg"></div>
    </div>
  </div></section>
  <section id="tab-gallery" class="tab"><div class="card">
    <h2 class="sec first">Image Gallery</h2>
    <div class="sub" id="galSum">&nbsp;</div>
    <div id="galMsg" class="msg"></div>
    <div id="slBox" class="slbox" hidden>
      <label for="slSel">Slideshow: a new picture every</label>
      <select id="slSel" class="inp"></select>
      <label class="chk"><input id="slShuf" type="checkbox"> Shuffle (random order instead of the order below)</label>
      <label for="slPortrait">Portrait pictures</label>
      <select id="slPortrait" class="inp">
        <option value="0">Fill the screen: I turn the frame clockwise</option>
        <option value="1">Fill the screen: I turn the frame counter-clockwise</option>
        <option value="2">Keep upright, smaller (no turning)</option>
      </select>
      <p class="note">With "fill the screen" a portrait picture is drawn sideways so that it fills the whole panel: you turn the frame to look at it. If it ends up upside down, choose the other direction.</p>
      <div id="slMsg" class="msg"></div>
      <p class="note">The Slideshow page shows the pictures that are switched on. Press G1 there for the next picture.</p>
    </div>
    <div id="galList" class="gal"></div>
    <div class="btns"><button id="galRescan" class="btn alt">Rescan card</button></div>
    <p class="note">Tap "In slideshow" or "Hidden" on a picture to choose whether the slideshow shows it. Add pictures in the Upload Image tab. Pictures are the PNG files in the /images folder on the card. A picture that is switched off stays on the card but is skipped by the slideshow. An original (JPEG) with the same name in /masters is kept so the picture can be edited again later.</p>
  </div></section>

  <section id="tab-wifi" class="tab"><div class="card">
    <h2 class="sec first">Home network</h2>
    <div class="big" id="wifiNow">Not set</div>

    <h2 class="sec">Networks the device found</h2>
    <div id="scanList" class="list"></div>
    <p class="note" id="scanMeta">&nbsp;</p>

    <h2 class="sec">Connect to</h2>
    <label for="wSsid">Network name</label>
    <input id="wSsid" class="inp" maxlength="32" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false">
    <label for="wPass">Password</label>
    <div class="line">
      <input id="wPass" class="inp" type="password" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false">
      <button id="wShow" class="btn alt" style="flex:none">Show</button>
    </div>
    <div class="btns"><button id="wSave" class="btn">Save network</button><button id="wForget" class="btn alt">Forget</button></div>
    <div id="wMsg" class="msg"></div>
    <p class="note">The device can't test the network while its own Wi-Fi is on. It connects when you leave this mode; if that fails, the Weather page says why. 2.4 GHz networks only. To refresh the list, leave the Connect page and open it again.</p>
  </div></section>

  <!-- ===== LED ===== -->
  <section id="tab-led" class="tab"><div class="card">
    <h2 class="sec first">RGB LED</h2>
    <div class="btns" style="margin-top:0"><button id="ledOn" class="btn">On</button><button id="ledOff" class="btn alt">Off</button></div>
    <label for="ledColor">Color</label>
    <input type="color" id="ledColor" value="#0000ff">
    <div class="sw" id="swatches">
      <button style="background:#ff0000" data-c="#ff0000" title="red"></button>
      <button style="background:#00ff00" data-c="#00ff00" title="green"></button>
      <button style="background:#0000ff" data-c="#0000ff" title="blue"></button>
      <button style="background:#ffff00" data-c="#ffff00" title="yellow"></button>
      <button style="background:#ff00ff" data-c="#ff00ff" title="magenta"></button>
      <button style="background:#ffffff" data-c="#ffffff" title="white"></button>
    </div>
    <label for="ledBr">Brightness <span id="brVal"></span></label>
    <input type="range" id="ledBr" min="1" max="255" value="64">
    <p class="note">This colour is a manual colour: while it is on, it replaces the status light below. Press Off to give the status light back.</p>

    <h2 class="sec">Status light</h2>
    <div id="stBox" hidden>
      <label class="chk"><input id="stOn" type="checkbox"> Let the LED show what the device is doing</label>
      <label for="stPct">Status light brightness <span id="stVal"></span></label>
      <input type="range" id="stPct" min="5" max="100" step="5" value="15">
      <div id="stMsg" class="msg"></div>
    </div>
  </div></section>

  <!-- ===== Tests ===== -->
  <section id="tab-instructions" class="tab"><div class="card help">
    <h2 class="sec first">How it works</h2>
    <p>The screen is an e-paper. It keeps showing its last picture even when the device has no power at all, and every change takes a few seconds and flashes. So a screen that does not change is not always a frozen device: it may be switched off, or its battery may be empty.</p>

    <h2 class="sec">The pages on the screen</h2>
    <p>There are three pages, always in this order: <b>Weather</b> &rarr; <b>Slideshow</b> &rarr; <b>Connect to Device</b> &rarr; and then Weather again.</p>
    <ul>
      <li><b>Weather</b>: the outdoor weather now and for the morning, afternoon and night (a TOMORROW tag shows when the forecast is for tomorrow), and the temperature and humidity measured inside the device. It updates by itself every 3, 6, 12 or 24 hours, as you chose on the <b class="t">Weather</b> tab.</li>
      <li><b>Slideshow</b>: your pictures from the SD card, one at a time. Each picture stays for 1, 3, 12 or 24 hours, as you chose in the <b class="t">Image Gallery</b> tab. The weather is not updated while this page is open; it catches up when you return to the Weather page.</li>
      <li><b>Connect to Device</b>: the device starts its own Wi-Fi so that you can open this page. The screen shows the Wi-Fi name, its password and a QR code.</li>
    </ul>

    <h2 class="sec">The three buttons</h2>
    <ul>
      <li><b>G10</b> (down): next page.</li>
      <li><b>G9</b> (up): previous page.</li>
      <li><b>G1</b> (top): on the Weather page, update the weather now; on the Slideshow page, show the next picture. It does nothing on the Connect to Device page.</li>
    </ul>
    <p>A button pressed while the screen is refreshing or the weather is being updated is ignored: wait until it is done.</p>

    <h2 class="sec">The Power button</h2>
    <ul>
      <li><b>Switched off:</b> press it once to switch the device on.</li>
      <li><b>Single click</b> while it is on: restarts the device. The Weather page comes back; if you were already on it you will hardly notice.</li>
      <li><b>Double click:</b> switches the device off. The screen keeps its last picture, so nothing seems to happen.</li>
      <li><b>Long press</b> (about one second): the device waits to be programmed from a computer, and the screen does not change. If that happens by accident, double-click to switch it off and press once to switch it on again.</li>
    </ul>

    <h2 class="sec">The status light</h2>
    <p>The small LED shows what the device is doing. You can switch it off, or make it dimmer, under <b class="u">Status light</b> in the <b class="t">LED</b> tab.</p>
    <ul>
      <li><b>One long white pulse</b>: a button press was received.</li>
      <li><b>Pink blinking</b>: the screen is refreshing.</li>
      <li><b>Blue and green pulses, taking turns</b>: the weather is being updated.</li>
      <li><b>Fast blue blinking</b>: a picture is being uploaded.</li>
      <li><b>Red and yellow flashing</b>, for ten seconds: something went wrong. A weather update failed, or the SD card cannot be read.</li>
      <li><b>Slow white pulse</b>: the Wi-Fi of the device is on and waits for your phone. <b>Steady white</b>: a phone is connected.</li>
      <li><b>Slow yellow pulse</b>: the battery is charging. <b>Steady green</b>: it is charged. Both stay on for as long as the charger is connected.</li>
      <li><b>One short red flash every 30 seconds</b>: the battery is low (15 % or less).</li>
    </ul>
    <p>If several of these apply at the same time, the first in this list wins: error, button, screen refresh, weather update, upload, low battery, Wi-Fi, charging. A colour that you set yourself in the <b class="t">LED</b> tab replaces the status light until you press <b class="u">Off</b>.</p>

    <h2 class="sec">Battery and charging</h2>
    <p>Charge the device through its USB connector. The symbol in the top bar shows the charge (a plug while it charges; the level is an estimate). When the battery is nearly empty the device shows a large crossed-out battery and switches itself off to protect the battery. Charge it, then press the Power button once. If the battery is still too low it shows the symbol again and switches off again.</p>
    <p>To make the battery last, the device sleeps whenever there is nothing to do. It wakes up at once when you press a button, and by itself when an update or a new picture is due. It stays awake while it is on a charger and while the Connect to Device page is open.</p>

    <h2 class="sec">Setting it up from your phone</h2>
    <p>Go to the Connect to Device page (press G10 twice from the Weather page), join the Wi-Fi shown on the screen (scan the QR code or type the password) and open the address shown: that is this page. After a few minutes without activity the device closes its Wi-Fi by itself and returns to the Weather page.</p>
    <ol>
      <li>On the <b class="t">Device</b> tab press <b class="u">Sync clock now</b>, so that the device knows the time. After that it corrects its clock by itself from the internet whenever it updates the weather.</li>
      <li>On the <b class="t">Weather</b> tab choose your place (<b class="u">Search</b> by name, or enter the coordinates) and how often to update (<b class="u">Fetch every</b>), and save.</li>
      <li>On the <b class="t">Wi-Fi</b> tab choose your home network, enter its password and press <b class="u">Save network</b>. The device joins your home network only for a moment, to fetch the weather.</li>
    </ol>

    <h2 class="sec">Adding pictures</h2>
    <p>You need a microSD card in the device (see below). Open the <b class="t">Upload Image</b> tab.</p>
    <ol>
      <li>Choose a photo (<b class="u">Choose a photo</b>): JPEG, PNG or TIFF. TIFF only opens in Safari. Camera raw files (ARW, CR2, NEF, DNG and so on) are not accepted: export them as JPEG first.</li>
      <li>Adjust it: <b class="u">Landscape</b> or <b class="u">Portrait</b> frame, <b class="u">Rotate</b>, <b class="u">Fill</b> or <b class="u">Fit</b>, zoom, and brightness, contrast, saturation, warmth and tint. Drag the picture to move it. <b class="u">Colours</b> chooses how the photo is reduced to the six colours of the screen (the default is Floyd&ndash;Steinberg); the preview shows the result.</li>
      <li>Press <b class="u">Save</b>. The picture is stored on the card and appears in the <b class="t">Image Gallery</b> tab. You can give it a name first (<b class="u">Name</b>).</li>
    </ol>
    <p>A picture that is not exactly the shape of the screen is scaled to fit, with white borders. A portrait picture is drawn turned, so that it fills the whole screen: turn the frame to look at it. If it ends up upside down, choose the other direction under <b class="u">Portrait pictures</b> in the <b class="t">Image Gallery</b> tab (or choose to keep it upright, smaller).</p>

    <h2 class="sec">Choosing and ordering pictures</h2>
    <p>In the <b class="t">Image Gallery</b> tab every picture has a switch: <b class="u">In slideshow</b> (it is shown) or <b class="u">Hidden</b> (it stays on the card but is skipped). <b class="u">Up</b> and <b class="u">Down</b> change the order. <b class="u">Edit</b> opens the picture in the editor again; the original is kept, so nothing is lost. <b class="u">Delete</b> removes it, after asking. At the top you choose how often the slideshow changes (<b class="u">Slideshow: a new picture every</b>) and whether the order is random (<b class="u">Shuffle</b>). Press <b class="u">Rescan card</b> after you changed the card on a computer.</p>

    <h2 class="sec">The SD card</h2>
    <p>Use a card formatted as FAT32 (on a Mac choose the "Master Boot Record" scheme; cards of 32 GB or less work best). exFAT and NTFS cards are not supported. The card symbol in the top bar: white = ready, yellow with an exclamation mark = a card is in but cannot be read, struck through in red = no card.</p>

    <h2 class="sec">If something does not work</h2>
    <ul>
      <li><b>The screen does not change at all:</b> the device may be switched off or the battery empty. Press the Power button once, and charge the device if you see the crossed-out battery.</li>
      <li><b>The weather shows an error:</b> the screen says why. Check the <b class="t">Wi-Fi</b> and <b class="t">Weather</b> tabs, and that your home Wi-Fi is in range. If the device could not get online it does not keep trying (that would only use up the battery): it waits until you save the Wi-Fi settings, or press G1 on the Weather page. After an error from the weather service it tries again after an hour.</li>
      <li><b>The slideshow has nothing to show:</b> the screen says why: no card, a card that cannot be read, no pictures, or none switched on.</li>
      <li><b>This page does not open:</b> go to the Connect to Device page again and rejoin the Wi-Fi of the device. The device closes it after a few minutes without activity.</li>
    </ul>
  </div></section>
  <section id="tab-tests" class="tab"><div class="card">
    <h2 class="sec first">Diagnostics</h2>
    <p class="note" style="margin:0">Checks what this phone can do while it is connected to the device's Wi-Fi.</p>
    <div class="btns"><button id="diagBtn" class="btn">Run tests</button></div>
    <ul id="diagList"></ul>
    <details id="diagBox" hidden><summary>Plain text, for sharing</summary><pre id="diagText"></pre></details>
  </div>
  <div class="card" style="margin-top:14px">
    <h2 class="sec first">microSD card</h2>
    <p class="note" style="margin:0">Writes a 32 KB file, reads it back and deletes it. The card shares its data lines with the screen, so leave this page afterwards: if the Weather page then redraws normally, the sharing works.</p>
    <div class="btns"><button id="sdBtn" class="btn">Test SD card</button></div>
    <ul id="sdList"></ul>
  </div></section>
</main>
<div id="pop" class="pop" hidden role="dialog" aria-modal="true" aria-labelledby="popTitle">
  <div class="popcard"><div class="tick">&#10003;</div><h3 id="popTitle"></h3><p id="popText"></p><button id="popOk" class="btn">OK</button></div>
</div>
<div id="cf" class="pop" hidden role="alertdialog" aria-modal="true" aria-labelledby="cfTitle">
  <div class="popcard"><div class="tick warn">!</div><h3 id="cfTitle"></h3><p id="cfText"></p>
    <div class="two"><button id="cfCancel" class="btn alt">Cancel</button><button id="cfOk" class="btn bad">Delete</button></div></div>
</div>
<div id="pv" class="pop" hidden role="dialog" aria-modal="true" aria-labelledby="pvTitle">
  <div class="popcard wide"><h3 id="pvTitle"></h3><img id="pvImg" alt=""><p style="margin:12px 0"><a id="pvOrig" target="_blank" rel="noopener" hidden>Open the original (JPEG)</a></p><button id="pvClose" class="btn">Close</button></div>
</div>
<footer id="idle">&nbsp;</footer>

<script id="pcimg">
// PCImg: the picture functions (no page, no canvas): the six panel colours, tone adjustment, three ways of reducing a
// picture to those colours, the PNG notes that remember how a picture was edited, and the framing maths.
// The "panel" quantiser is a port of the one in M5Stack's factory web page (M5PaperColor-UserDemo, MIT licence),
// which imitates the panel driver's own dithering.
var PCImg = (function(){
  'use strict';
  var PURE = [[0,0,0],[255,255,255],[255,255,0],[255,0,0],[0,0,255],[0,255,0]];     // what the panel driver is given
  var REAL = [[0,0,0],[255,255,255],[255,243,56],[191,0,0],[0,0,255],[67,138,28]];  // what the six inks really look like
  function clamp8(v){ return v < 0 ? 0 : v > 255 ? 255 : v; }

  // ---------- tone: brightness, contrast, saturation, warmth, tint, each -100..100 ----------
  // Warmth moves red up and blue down (cooler when negative); tint moves green down and red and blue up (pinker when positive,
  // greener when negative). +-100 is about +-40 steps of 255 on the strongest channel.
  function adjust(d, n, o){
    var b = o.b || 0, c = o.c || 0, s = o.s || 0, w = o.w || 0, t = o.t || 0;
    if (!b && !c && !s && !w && !t) return;
    var i, p, lr = null, lg = null, lb = null;
    if (b || c || w || t){                         // these act on each channel alone: three 256-entry tables do it
      var cf = c >= 0 ? 1 + c / 50 : 1 + c / 100, add = b * 1.28;
      var dr = w * 0.4 + t * 0.15, dg = w * 0.1 - t * 0.4, db = -w * 0.4 + t * 0.15;
      lr = new Uint8Array(256); lg = new Uint8Array(256); lb = new Uint8Array(256);
      for (i = 0; i < 256; i++){
        var base = (i - 128) * cf + 128 + add;
        lr[i] = Math.round(clamp8(base + dr)); lg[i] = Math.round(clamp8(base + dg)); lb[i] = Math.round(clamp8(base + db));
      }
    }
    var sq = Math.round((s >= 0 ? 1 + s / 50 : 1 + s / 100) * 256);       // saturation in 1/256ths: whole-number arithmetic is much faster
    for (i = 0, p = 0; i < n; i++, p += 4){
      var r = d[p], g = d[p + 1], bl = d[p + 2];
      if (lr){ r = lr[r]; g = lg[g]; bl = lb[bl]; }
      if (s){
        var l = (77 * r + 150 * g + 29 * bl) >> 8;
        r = l + (((r - l) * sq) >> 8); g = l + (((g - l) * sq) >> 8); bl = l + (((bl - l) * sq) >> 8);
        r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; bl = bl < 0 ? 0 : bl > 255 ? 255 : bl;
      }
      d[p] = r; d[p + 1] = g; d[p + 2] = bl;
    }
  }

  // ---------- reducing to the six colours (the result holds the PURE palette colours) ----------
  // "Nearest" is judged the way the eye does (CIE Lab), against the real colours of the inks. (With plain RGB distance
  // a mid-grey would be nearest to the green ink, and every grey wall or sky would come out speckled with green.)
  var LIN = new Float32Array(256);
  for (var li = 0; li < 256; li++){ var lc = li / 255; LIN[li] = lc <= 0.04045 ? lc / 12.92 : Math.pow((lc + 0.055) / 1.055, 2.4); }
  function labF(t){ return t > 0.008856 ? Math.cbrt(t) : 7.787 * t + 16 / 116; }
  function lab(r, g, b, out){
    var R = LIN[(r + 0.5) | 0], G = LIN[(g + 0.5) | 0], B = LIN[(b + 0.5) | 0];
    var fx = labF((0.4124564 * R + 0.3575761 * G + 0.1804375 * B) / 0.95047), fy = labF(0.2126729 * R + 0.7151522 * G + 0.072175 * B), fz = labF((0.0193339 * R + 0.119192 * G + 0.9503041 * B) / 1.08883);
    out[0] = 116 * fy - 16; out[1] = 500 * (fx - fy); out[2] = 200 * (fy - fz);
  }
  var LAB_REAL = [], LT = [0, 0, 0];
  for (var lk = 0; lk < 6; lk++){ var lo = [0, 0, 0]; lab(REAL[lk][0], REAL[lk][1], REAL[lk][2], lo); LAB_REAL.push(lo); }
  function nearestIdx(r, g, b){
    lab(r, g, b, LT);
    var best = 0, bd = 1e9;
    for (var k = 0; k < 6; k++){
      var c = LAB_REAL[k], dl = LT[0] - c[0], da = LT[1] - c[1], db = LT[2] - c[2], dd = dl * dl + da * da + db * db;
      if (dd < bd){ bd = dd; best = k; }
    }
    return best;
  }
  function put(d, p, k){ var c = PURE[k]; d[p] = c[0]; d[p + 1] = c[1]; d[p + 2] = c[2]; d[p + 3] = 255; }

  function nearest(d, w, h){
    for (var i = 0, p = 0, n = w * h; i < n; i++, p += 4) put(d, p, nearestIdx(d[p], d[p + 1], d[p + 2]));
  }

  // Floyd-Steinberg error diffusion, alternating direction on every row.
  function floydSteinberg(d, w, h){
    var cur = new Float32Array((w + 2) * 3), nxt = new Float32Array((w + 2) * 3);
    for (var y = 0; y < h; y++){
      var ltr = (y & 1) === 0, x0 = ltr ? 0 : w - 1, x1 = ltr ? w : -1, dx = ltr ? 1 : -1;
      for (var x = x0; x !== x1; x += dx){
        var p = (y * w + x) * 4, e = (x + 1) * 3;
        var r = clamp8(d[p] + cur[e]), g = clamp8(d[p + 1] + cur[e + 1]), b = clamp8(d[p + 2] + cur[e + 2]);
        var k = nearestIdx(r, g, b), q = REAL[k], er = r - q[0], eg = g - q[1], eb = b - q[2];
        var f = e + dx * 3, bk = e - dx * 3;        // next pixel on this row, and the one behind us on the next row
        cur[f] += er * 7 / 16; cur[f + 1] += eg * 7 / 16; cur[f + 2] += eb * 7 / 16;
        nxt[bk] += er * 3 / 16; nxt[bk + 1] += eg * 3 / 16; nxt[bk + 2] += eb * 3 / 16;
        nxt[e] += er * 5 / 16; nxt[e + 1] += eg * 5 / 16; nxt[e + 2] += eb * 5 / 16;
        nxt[f] += er / 16; nxt[f + 1] += eg / 16; nxt[f + 2] += eb / 16;
        put(d, p, k);
      }
      var t = cur; cur = nxt; nxt = t; nxt.fill(0);
    }
  }

  // The panel driver's own dithering, as imitated by the factory web page: pixels are taken in pairs, a spatial noise
  // bias is added, and the pair of palette colours that fits the two pixels best is chosen. Same constants, same result.
  var QD = 200, STEP = 129 * 127, DIFF = Math.floor(STEP / 3), XS = 127 * 29, YS = 129 * 48;
  var FD = new Int32Array(24), SD = new Int32Array(24);
  function pairColors(r0, g0, b0, r1, g1, b1, out){
    var i, j, c;
    for (i = 0; i < 6; i++){
      c = PURE[i];
      FD[i * 4] = r0 - c[0]; FD[i * 4 + 1] = g0 - c[1]; FD[i * 4 + 2] = b0 - c[2]; FD[i * 4 + 3] = FD[i * 4] * FD[i * 4] + FD[i * 4 + 1] * FD[i * 4 + 1] + FD[i * 4 + 2] * FD[i * 4 + 2];
      SD[i * 4] = r1 - c[0]; SD[i * 4 + 1] = g1 - c[1]; SD[i * 4 + 2] = b1 - c[2]; SD[i * 4 + 3] = SD[i * 4] * SD[i * 4] + SD[i * 4 + 1] * SD[i * 4 + 1] + SD[i * 4 + 2] * SD[i * 4 + 2];
    }
    var best = Infinity, bi = 1, bj = 1;
    for (i = 0; i < 6; i++) for (j = 0; j < 6; j++){
      var dr = FD[i * 4] + SD[j * 4], dg = FD[i * 4 + 1] + SD[j * 4 + 1], db = FD[i * 4 + 2] + SD[j * 4 + 2];
      var dist = (dr * dr + dg * dg + db * db) + FD[i * 4 + 3] + SD[j * 4 + 3];
      if (dist < best){ best = dist; bi = i; bj = j; }
    }
    out[0] = bi; out[1] = bj;
  }
  function panelQuantize(d, w, h){
    var pr = new Int32Array(2);
    for (var y = 0; y < h; y++){
      var base = (y * YS) % STEP, r0 = 0, g0 = 0, b0 = 0, prev = -1;
      for (var x = 0; x <= w; x++){
        var r = 128, g = 128, b = 128, cur = -1;
        if (x < w){ cur = (y * w + x) * 4; r = d[cur]; g = d[cur + 1]; b = d[cur + 2]; }
        base -= XS; if (base < 0) base += STEP;
        var bR = base, bG = base - DIFF; if (bG < 0) bG += STEP;
        var bB = bG - DIFF; if (bB < 0) bB += STEP;
        bB = bB * 2 - (STEP - 1); bG = bG * 2 - (STEP - 1); bR = bR * 2 - (STEP - 1);
        var bias = bR + bG + bB;
        bias = (bias * QD) >> 16; bR = (bR * QD) >> 16; bG = (bG * QD) >> 16; bB = (bB * QD) >> 16;
        r = clamp8(r + bias + bR); g = clamp8(g + bias + bG); b = clamp8(b + bias + bB);
        if (x & 1){
          pairColors(r0, g0, b0, r, g, b, pr);
          if (prev >= 0) put(d, prev, pr[0]);
          if (cur >= 0) put(d, cur, pr[1]);
        } else { r0 = r; g0 = g; b0 = b; prev = cur; }
      }
    }
  }
  function quantize(mode, d, w, h){
    if (mode === 'fs') floydSteinberg(d, w, h);
    else if (mode === 'nearest') nearest(d, w, h);
    else panelQuantize(d, w, h);
  }
  // For the on-screen preview: show each palette colour the way its ink really looks.
  function toInk(d, w, h){
    for (var i = 0, p = 0, n = w * h; i < n; i++, p += 4){
      var best = 0, bd = 1e9;
      for (var k = 0; k < 6; k++){
        var c = PURE[k], dr = d[p] - c[0], dg = d[p + 1] - c[1], db = d[p + 2] - c[2], dd = dr * dr + dg * dg + db * db;
        if (dd < bd){ bd = dd; best = k; }
      }
      d[p] = REAL[best][0]; d[p + 1] = REAL[best][1]; d[p + 2] = REAL[best][2];
    }
  }

  // ---------- PNG notes: how a picture was edited is stored inside the picture (a "tEXt" chunk) ----------
  var CRCT = (function(){ var t = new Uint32Array(256); for (var n = 0; n < 256; n++){ var c = n; for (var k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1); t[n] = c >>> 0; } return t; })();
  function crc32(u8, from, to){ var c = 0xFFFFFFFF; for (var i = from; i < to; i++) c = CRCT[(c ^ u8[i]) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }
  function be32(u8, o){ return ((u8[o] << 24) | (u8[o + 1] << 16) | (u8[o + 2] << 8) | u8[o + 3]) >>> 0; }
  function put32(u8, o, v){ u8[o] = v >>> 24; u8[o + 1] = (v >>> 16) & 255; u8[o + 2] = (v >>> 8) & 255; u8[o + 3] = v & 255; }
  var SIG = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
  function chunkType(u8, o){ return String.fromCharCode(u8[o + 4], u8[o + 5], u8[o + 6], u8[o + 7]); }
  function isPng(u8){
    if (!u8 || u8.length < 33) return false;
    for (var i = 0; i < 8; i++) if (u8[i] !== SIG[i]) return false;
    return be32(u8, 8) === 13 && chunkType(u8, 8) === 'IHDR';
  }
  function pngInsertText(u8, items){               // items: [[keyword, text], ...] -> a new PNG with those notes right after IHDR
    if (!isPng(u8)) return u8;
    var chunks = [], total = 0, i, j;
    for (i = 0; i < items.length; i++){
      var key = String(items[i][0]).replace(/[^\x20-\x7e]/g, '').slice(0, 79), text = String(items[i][1]);
      if (!key) continue;
      var len = key.length + 1 + text.length, ch = new Uint8Array(12 + len);
      put32(ch, 0, len); ch[4] = 0x74; ch[5] = 0x45; ch[6] = 0x58; ch[7] = 0x74;     // "tEXt"
      for (j = 0; j < key.length; j++) ch[8 + j] = key.charCodeAt(j);
      ch[8 + key.length] = 0;
      for (j = 0; j < text.length; j++) ch[9 + key.length + j] = text.charCodeAt(j) & 255;
      put32(ch, 8 + len, crc32(ch, 4, 8 + len));
      chunks.push(ch); total += ch.length;
    }
    var out = new Uint8Array(u8.length + total), o = 33;
    out.set(u8.subarray(0, 33), 0);
    for (i = 0; i < chunks.length; i++){ out.set(chunks[i], o); o += chunks[i].length; }
    out.set(u8.subarray(33), o);
    return out;
  }
  function pngReadText(u8){                        // {keyword: text} of the notes that come before the picture data
    var r = {};
    if (!isPng(u8)) return r;
    var o = 8;
    while (o + 12 <= u8.length){
      var len = be32(u8, o), type = chunkType(u8, o);
      if (type === 'IDAT' || type === 'IEND' || o + 12 + len > u8.length) break;
      if (type === 'tEXt'){
        var z = o + 8, end = o + 8 + len;
        while (z < end && u8[z] !== 0) z++;
        if (z < end && z > o + 8){
          var key = '', text = '', i;
          for (i = o + 8; i < z; i++) key += String.fromCharCode(u8[i]);
          for (i = z + 1; i < end; i++) text += String.fromCharCode(u8[i]);
          r[key] = text;
        }
      }
      o += 12 + len;
    }
    return r;
  }

  // ---------- the editing state, framing maths ----------
  function num(v, lo, hi, d){ v = +v; if (!isFinite(v)) return d; return v < lo ? lo : v > hi ? hi : v; }
  function sanitize(o){                            // whatever comes in (also from a note inside a file), this is safe to use
    o = (o && typeof o === 'object') ? o : {};
    return {
      orient: o.orient === 'port' ? 'port' : 'land',
      rot: ((Math.round(num(o.rot, -1e6, 1e6, 0)) % 4) + 4) % 4,
      mode: o.mode === 'fit' ? 'fit' : 'fill',
      zoom: Math.round(num(o.zoom, 1, 8, 1) * 1000) / 1000,
      cx: Math.round(num(o.cx, -4000, 4000, 0)), cy: Math.round(num(o.cy, -4000, 4000, 0)),
      b: Math.round(num(o.b, -100, 100, 0)), c: Math.round(num(o.c, -100, 100, 0)), s: Math.round(num(o.s, -100, 100, 0)),
      w: Math.round(num(o.w, -100, 100, 0)), t: Math.round(num(o.t, -100, 100, 0)),
      dither: (o.dither === 'panel' || o.dither === 'nearest') ? o.dither : 'fs',          // Floyd-Steinberg is the default (a missing or unknown value); an explicit 'panel' or 'nearest' is kept
      border: o.border === 'black' ? 'black' : 'white'
    };
  }
  function encodeSettings(st){ return JSON.stringify(sanitize(st)); }
  function decodeSettings(text){ try { var o = JSON.parse(text); return (o && typeof o === 'object') ? sanitize(o) : null; } catch(e){ return null; } }
  function frame(orient){ return orient === 'port' ? [400, 600] : [600, 400]; }
  // Everything needed to draw: the frame, the scale of the picture in it, and the pan limited so the picture never leaves gaps.
  function view(st, sw, sh){
    var f = frame(st.orient), W = f[0], H = f[1], odd = st.rot & 1, rw = odd ? sh : sw, rh = odd ? sw : sh;
    var base = st.mode === 'fit' ? Math.min(W / rw, H / rh) : Math.max(W / rw, H / rh), scale = base * st.zoom;
    var iw = rw * scale, ih = rh * scale, mx = Math.max(0, (iw - W) / 2), my = Math.max(0, (ih - H) / 2);
    return { W: W, H: H, scale: scale, rot: st.rot, iw: iw, ih: ih, cx: Math.max(-mx, Math.min(mx, st.cx)), cy: Math.max(-my, Math.min(my, st.cy)) };
  }

  // ---------- names and file kinds ----------
  function suggestName(fileName){                  // "IMG_1234_dithered.png" -> "IMG_1234"
    var n = String(fileName).replace(/\.[^.]*$/, ''), suffix = /[_\- ](dithered|dither|resized|nearest)$/i;
    while (suffix.test(n)) n = n.replace(suffix, '');
    if (n.normalize) n = n.normalize('NFD').replace(/[\u0300-\u036f]/g, '');
    n = n.replace(/[^A-Za-z0-9_\-]+/g, '_').replace(/_+/g, '_').replace(/^_+|_+$/g, '');
    return n.slice(0, 40) || 'picture';
  }
  // 'jpeg' | 'png' | 'tiff' | 'raw' | null, from the file name (for raw files) and the first bytes
  function kindOfFile(head, name){
    var ext = (/\.([A-Za-z0-9]+)$/.exec(String(name)) || [0, ''])[1].toLowerCase();
    if (/^(arw|cr2|cr3|crw|nef|nrw|orf|raf|rw2|rwl|dng|srf|sr2|pef|3fr|erf|kdc|mrw|x3f|iiq|raw)$/.test(ext)) return 'raw';
    if (head.length >= 3 && head[0] === 0xff && head[1] === 0xd8 && head[2] === 0xff) return 'jpeg';
    var sig = head.length >= 8;
    for (var i = 0; i < 8 && sig; i++) if (head[i] !== SIG[i]) sig = false;
    if (sig) return 'png';
    if (head.length >= 4 && ((head[0] === 0x49 && head[1] === 0x49 && head[2] === 0x2a && head[3] === 0) || (head[0] === 0x4d && head[1] === 0x4d && head[2] === 0 && head[3] === 0x2a))) return 'tiff';
    return null;
  }

  return { PURE: PURE, REAL: REAL, adjust: adjust, nearest: nearest, floydSteinberg: floydSteinberg, panelQuantize: panelQuantize, quantize: quantize, toInk: toInk,
           crc32: crc32, isPng: isPng, pngInsertText: pngInsertText, pngReadText: pngReadText, sanitize: sanitize, encodeSettings: encodeSettings, decodeSettings: decodeSettings,
           frame: frame, view: view, suggestName: suggestName, kindOfFile: kindOfFile };
})();
</script>
<script>
(function(){
  var $ = function(id){ return document.getElementById(id); };
  function pad(n){ return (n < 10 ? '0' : '') + n; }
  function hex2(n){ return (n < 16 ? '0' : '') + n.toString(16); }
  function fmtUtcFields(ms){            // format a Date's UTC fields (used for "shifted" epochs)
    var d = new Date(ms);
    return d.getUTCFullYear() + '-' + pad(d.getUTCMonth()+1) + '-' + pad(d.getUTCDate()) + ' ' +
           pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());
  }
  function fmtOffset(min){
    var sign = min < 0 ? '-' : '+', a = Math.abs(min);
    return 'UTC' + sign + pad(Math.floor(a/60)) + ':' + pad(a%60);
  }
  function fmtInterval(m){ return m < 60 ? m + ' min' : (m % 60 === 0 ? (m/60) + ' h' : m + ' min'); }
  function setMsg(id, text, cls){ var e = $(id); e.textContent = text; e.className = 'msg ' + (cls || ''); }
  function clear(e){ while (e.firstChild) e.removeChild(e.firstChild); }

  // Confirmation pop-up: closes with OK, a tap outside the card, Escape, or by itself after a few seconds.
  var POPUP_MS = window.PC_POPUP_MS || 5000, popTimer = null;
  function closePopup(){ clearTimeout(popTimer); $('pop').hidden = true; }
  function showPopup(title, text){
    $('popTitle').textContent = title;
    $('popText').textContent = text;
    $('pop').hidden = false;
    $('popOk').focus();
    clearTimeout(popTimer);
    popTimer = setTimeout(closePopup, POPUP_MS);
  }
  $('popOk').onclick = closePopup;
  $('pop').onclick = function(e){ if (e.target === this) closePopup(); };
  document.onkeydown = function(e){ if (e.key === 'Escape' || e.keyCode === 27){ closePopup(); closeConfirm(false); closePreview(); } };

  // The e-paper fonts only know plain ASCII, so labels are reduced to ASCII here.
  var SPECIAL = {'\u00f8':'o','\u00d8':'O','\u00df':'ss','\u00e6':'ae','\u00c6':'AE','\u0153':'oe','\u0152':'OE',
                 '\u0111':'d','\u0110':'D','\u0142':'l','\u0141':'L','\u0131':'i','\u00fe':'th','\u00de':'Th','\u00f0':'d','\u00d0':'D'};
  function asciiLabel(s){
    s = String(s).replace(/[\u00f8\u00d8\u00df\u00e6\u00c6\u0153\u0152\u0111\u0110\u0142\u0141\u0131\u00fe\u00de\u00f0\u00d0]/g, function(c){ return SPECIAL[c]; });
    if (s.normalize) s = s.normalize('NFD').replace(/[\u0300-\u036f]/g, '');
    return s.replace(/[^\x20-\x7e]/g, '').replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');
  }

  // ---------- state ----------
  var led = {on:false, r:0, g:0, b:255, br:64};
  var devEpoch = null, devOffsetMin = 0, devAt = 0;     // device clock as last reported
  var idleRem = null, idleAt = 0;
  var failures = 0;
  var S = null;                // settings as stored on the device
  var picked = null;           // location chosen on this page but not saved yet: {lat, lon, place}

  function setConn(ok){
    var c = $('conn');
    c.textContent = ok ? 'connected' : 'offline';
    c.className = 'chip ' + (ok ? 'ok' : 'bad');
    $('banner').hidden = ok || failures < 2;
  }

  function api(path, method){
    return fetch(path, {method: method || 'GET', cache: 'no-store'}).then(function(r){
      if (!r.ok){ var e = new Error('HTTP ' + r.status); e.status = r.status; throw e; }
      return r.json();
    }).then(function(j){ failures = 0; setConn(true); return j; },
            function(e){ if (!e.status) { failures++; setConn(false); } throw e; });
  }

  // ---------- tabs ----------
  var tabButtons = $('tabs').getElementsByTagName('button');
  var curTab = 'device';
  function showTab(name){
    if (name === 'images') name = 'gallery';           // (the old name of the tab: old links and bookmarks still work)
    var found = false;
    for (var i = 0; i < tabButtons.length; i++){
      var on = tabButtons[i].getAttribute('data-tab') === name;
      tabButtons[i].className = on ? 'on' : '';
      if (on) found = true;
    }
    if (!found) name = 'device';
    curTab = name;
    var tabs = document.getElementsByClassName('tab');
    for (var j = 0; j < tabs.length; j++) tabs[j].className = 'tab' + (tabs[j].id === 'tab-' + name ? ' on' : '');
    try { history.replaceState(null, '', '#' + name); } catch(e){}
    window.scrollTo(0, 0);
    if (name === 'gallery') loadGallery();
  }
  for (var t = 0; t < tabButtons.length; t++){
    tabButtons[t].onclick = function(){ showTab(this.getAttribute('data-tab')); };
  }

  // ---------- status (clock, battery, LED, profile) ----------
  function applyStatus(s, withLed){
    if (s.profile){ var p = $('prof'); p.textContent = s.profile === 'TEST' ? 'TEST' : ''; p.hidden = s.profile !== 'TEST'; }
    if (withLed && s.led){
      led.on = !!s.led.on; led.r = s.led.r; led.g = s.led.g; led.b = s.led.b; led.br = s.led.br;
      $('ledColor').value = '#' + hex2(led.r) + hex2(led.g) + hex2(led.b);
      $('ledBr').value = led.br; $('brVal').textContent = led.br;
      $('ledOn').className = 'btn' + (led.on ? '' : ' alt');
      $('ledOff').className = 'btn' + (led.on ? ' alt' : '');
    }
    if (s.time){
      if (s.time.ok){ devEpoch = s.time.epoch; devOffsetMin = s.time.offsetMin; devAt = Date.now(); }
      else devEpoch = null;
      $('tz').textContent = (s.time.tz && !/^(Etc\/)?UTC$/i.test(s.time.tz) ? s.time.tz + ' ' : '') + fmtOffset(s.time.offsetMin);
    }
    if (s.battery){
      $('bat').textContent = s.battery.ok ? (s.battery.pct + '% \u00b7 ' + (s.battery.mv/1000).toFixed(2) + ' V' + (s.battery.charging ? ' \u00b7 charging' : '')) : 'n/a';
    }
    if (s.sd) renderSd(s.sd);
    if (s.idle){ idleRem = s.idle.remainingSec; idleAt = Date.now(); }
    tick();                      // show new values right away instead of waiting for the next second
  }

  function fmtSize(mb){ return mb >= 1024 ? (mb / 1024).toFixed(1) + ' GB' : mb + ' MB'; }
  function renderSd(sd){
    var e = $('sdState'), t, cls;
    if (sd.state === 'ok'){ t = (sd.type ? sd.type + ' \u00b7 ' : '') + fmtSize(sd.mb) + ' card'; cls = 'ok'; }
    else if (sd.state === 'error'){ t = sd.error || "Card can't be used"; cls = 'bad'; }
    else { t = 'No card'; cls = 'na'; }
    e.textContent = t;
    e.className = 'wrap ' + cls;
  }

  function setSync(text, cls){ var e = $('syncState'); e.textContent = text; e.className = 'sub ' + cls; }

  function tick(){
    var now = new Date();
    $('phoneTime').textContent = fmtUtcFields(now.getTime() - now.getTimezoneOffset()*60000);
    if (devEpoch === null){
      $('devTime').textContent = 'not set';
      setSync('not synced yet', 'bad');
    } else {
      var ms = (devEpoch + (Date.now() - devAt)/1000) * 1000;
      var diff = Math.round((ms - Date.now())/1000);
      $('devTime').textContent = fmtUtcFields(ms + devOffsetMin*60000);
      if (Math.abs(diff) <= 2) setSync('in sync with this phone', 'ok');
      else setSync('off by ' + diff + ' s', 'bad');
    }
    if (idleRem !== null){
      var left = Math.max(0, idleRem - Math.floor((Date.now() - idleAt)/1000));
      $('idle').textContent = 'The Wi-Fi closes in ' + Math.floor(left/60) + ':' + pad(left%60) + ' if nothing is touched.';
    }
  }

  function syncTime(auto){
    var now = new Date();
    var tz = '';
    try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone || ''; } catch(e){}
    var q = '/api/time?epochMs=' + now.getTime() + '&offsetMin=' + (-now.getTimezoneOffset()) +
            '&tz=' + encodeURIComponent(tz);
    $('syncBtn').disabled = true;
    return api(q, 'POST').then(function(s){
      applyStatus(s, false);
      $('syncMsg').textContent = (auto ? 'Clock synced automatically' : 'Clock synced') + ' at ' + pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds()) + '.';
    }).catch(function(){ $('syncMsg').textContent = 'Sync failed.'; })
      .then(function(){ $('syncBtn').disabled = false; });
  }
  $('syncBtn').onclick = function(){ syncTime(false); };

  // ---------- LED ----------
  var ledTimer = null;
  function pushLed(){
    clearTimeout(ledTimer);
    ledTimer = setTimeout(function(){
      var q = '/api/led?on=' + (led.on ? 1 : 0) + '&r=' + led.r + '&g=' + led.g + '&b=' + led.b + '&br=' + led.br;
      api(q, 'POST').then(function(s){ applyStatus(s, true); }).catch(function(){});
    }, 120);
  }
  function setColorHex(h){
    led.r = parseInt(h.substr(1,2),16); led.g = parseInt(h.substr(3,2),16); led.b = parseInt(h.substr(5,2),16);
    $('ledColor').value = h;
  }
  $('ledOn').onclick  = function(){ led.on = true;  pushLed(); };
  $('ledOff').onclick = function(){ led.on = false; pushLed(); };
  $('ledColor').oninput = function(){ setColorHex(this.value); led.on = true; pushLed(); };
  $('ledBr').oninput = function(){ led.br = parseInt(this.value,10); $('brVal').textContent = led.br; led.on = true; pushLed(); };
  var sw = $('swatches').getElementsByTagName('button');
  for (var i = 0; i < sw.length; i++){
    sw[i].onclick = function(){ setColorHex(this.getAttribute('data-c')); led.on = true; pushLed(); };
  }

  // ---------- Weather tab ----------
  var WMO = {0:'Clear',1:'Mostly clear',2:'Partly cloudy',3:'Cloudy',45:'Fog',48:'Fog',51:'Drizzle',53:'Drizzle',55:'Drizzle',56:'Drizzle',57:'Drizzle',
             61:'Rain',63:'Rain',65:'Rain',66:'Rain',67:'Rain',71:'Snow',73:'Snow',75:'Snow',77:'Snow',80:'Showers',81:'Showers',82:'Showers',
             85:'Snow showers',86:'Snow showers',95:'Thunderstorm',96:'Thunderstorm',99:'Thunderstorm'};

  function coordText(lat, lon){ return lat.toFixed(2) + ', ' + lon.toFixed(2); }

  function renderSettings(){
    if (!S) return;
    var l = S.location, w = S.wifi;
    $('locNow').textContent = l.set ? l.place + '  (' + coordText(l.lat, l.lon) + ')' : 'Not set';
    var sel = $('fetchSel'), keep = sel.value;
    clear(sel);
    for (var i = 0; i < S.fetch.options.length; i++){
      var m = S.fetch.options[i], o = document.createElement('option');
      o.value = m; o.textContent = fmtInterval(m);
      if (m === S.fetch.min) o.selected = true;
      sel.appendChild(o);
    }
    renderSlide();
    renderStatusLed();
    $('wifiNow').textContent = w.ssid ? w.ssid + (w.passSet ? '  \u00b7 password saved' : '  \u00b7 no password') : 'Not set';
    $('wPass').placeholder = (w.ssid && w.passSet && $('wSsid').value === w.ssid) ? 'Leave empty to keep the saved password' : '';
  }

  // ----- slideshow settings (in the Images tab): saved as soon as they are changed -----
  function renderSlide(){
    var box = $('slBox');
    if (!S || !S.slide){ box.hidden = true; return; }
    var sel = $('slSel');
    clear(sel);
    for (var i = 0; i < S.slide.options.length; i++){
      var m = S.slide.options[i], o = document.createElement('option');
      o.value = m; o.textContent = fmtInterval(m);
      if (m === S.slide.min) o.selected = true;
      sel.appendChild(o);
    }
    $('slShuf').checked = !!S.slide.shuffle;
    $('slPortrait').value = String(S.slide.portrait || 0);
    box.hidden = false;
  }
  function saveSlide(query){
    $('slSel').disabled = true; $('slShuf').disabled = true; $('slPortrait').disabled = true;
    setMsg('slMsg', '', '');
    api('/api/settings?' + query, 'POST').then(function(s){
      S = s; renderSlide();
      setMsg('slMsg', 'Saved. The Slideshow page uses this from the next picture on.', 'ok');
    }).catch(function(e){
      renderSlide();                                             // back to what the device really has
      setMsg('slMsg', e.status === 400 ? 'The device rejected that.' : 'Could not reach the device.', 'bad');
    }).then(function(){ $('slSel').disabled = false; $('slShuf').disabled = false; $('slPortrait').disabled = false; });
  }
  $('slSel').onchange = function(){ saveSlide('slideMin=' + encodeURIComponent(this.value)); };
  $('slShuf').onchange = function(){ saveSlide('shuffle=' + (this.checked ? 1 : 0)); };
  $('slPortrait').onchange = function(){ saveSlide('portrait=' + encodeURIComponent(this.value)); };

  // ----- status light settings (in the LED tab): saved as soon as they are changed -----
  function renderStatusLed(){
    var box = $('stBox');
    if (!S || !S.statusLed){ box.hidden = true; return; }
    $('stOn').checked = !!S.statusLed.on;
    $('stPct').value = String(S.statusLed.pct);
    $('stVal').textContent = S.statusLed.pct + ' %';
    box.hidden = false;
  }
  function saveStatusLed(query){
    $('stOn').disabled = true; $('stPct').disabled = true;
    setMsg('stMsg', '', '');
    api('/api/settings?' + query, 'POST').then(function(s){
      S = s; renderStatusLed();
      setMsg('stMsg', 'Saved.', 'ok');
    }).catch(function(e){
      renderStatusLed();                                           // back to what the device really has
      setMsg('stMsg', e.status === 400 ? 'The device rejected that.' : 'Could not reach the device.', 'bad');
    }).then(function(){ $('stOn').disabled = false; $('stPct').disabled = false; });
  }
  $('stOn').onchange = function(){ saveStatusLed('ledStatus=' + (this.checked ? 1 : 0)); };
  $('stPct').oninput = function(){ $('stVal').textContent = this.value + ' %'; };                // (only the label moves while the slider is dragged)
  $('stPct').onchange = function(){ saveStatusLed('ledPct=' + encodeURIComponent(this.value)); };

  function renderPicked(){
    var box = $('selBox');
    if (!picked){ box.hidden = true; return; }
    $('selName').textContent = picked.place;
    $('selCoords').textContent = '(' + coordText(picked.lat, picked.lon) + ')';
    box.hidden = false;
  }

  function pick(lat, lon, place){
    place = asciiLabel(place).substr(0, 36);
    if (!place) place = coordText(lat, lon);
    picked = {lat: Math.round(lat*100000)/100000, lon: Math.round(lon*100000)/100000, place: place};
    renderPicked();
  }

  function searchCity(){
    var q = $('q').value.replace(/^\s+|\s+$/g, '');
    if (q.length < 2){ setMsg('qMsg', 'Type at least two letters.', 'na'); return; }
    setMsg('qMsg', 'Searching\u2026', 'na');
    clear($('qList'));
    $('qBtn').disabled = true;
    fetch('https://geocoding-api.open-meteo.com/v1/search?name=' + encodeURIComponent(q) + '&count=8&language=en&format=json', {cache:'no-store'})
      .then(function(r){ return r.json(); })
      .then(function(j){
        var res = j.results || [];
        if (!res.length){ setMsg('qMsg', 'No match found.', 'bad'); return; }
        setMsg('qMsg', '', '');
        var list = $('qList');
        res.forEach(function(r){
          var parts = [r.name];
          if (r.admin1 && r.admin1 !== r.name) parts.push(r.admin1);
          if (r.country_code) parts.push(r.country_code);
          var label = parts.join(', ');
          var it = document.createElement('div');
          it.className = 'item';
          var nm = document.createElement('span'); nm.className = 'nm'; nm.textContent = label;
          var sm = document.createElement('small'); sm.textContent = coordText(r.latitude, r.longitude);
          it.appendChild(nm); it.appendChild(sm);
          it.onclick = function(){
            var all = list.getElementsByClassName('item');
            for (var k = 0; k < all.length; k++) all[k].className = 'item';
            it.className = 'item sel';
            pick(r.latitude, r.longitude, label);
          };
          list.appendChild(it);
        });
      })
      .catch(function(){
        setMsg('qMsg', "City search needs mobile data on this phone. You can enter coordinates instead.", 'bad');
        $('manual').open = true;
      })
      .then(function(){ $('qBtn').disabled = false; });
  }
  $('qBtn').onclick = searchCity;
  $('q').onkeydown = function(e){ if (e.key === 'Enter' || e.keyCode === 13){ e.preventDefault(); searchCity(); } };

  $('mUse').onclick = function(){
    var lat = parseFloat($('mLat').value.replace(',', '.')), lon = parseFloat($('mLon').value.replace(',', '.'));
    if (!isFinite(lat) || !isFinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180){
      setMsg('mMsg', 'Latitude must be between -90 and 90, longitude between -180 and 180.', 'bad'); return;
    }
    setMsg('mMsg', '', '');
    pick(lat, lon, $('mName').value);
  };

  $('wxSave').onclick = function(){
    var q = '/api/settings?fetchMin=' + encodeURIComponent($('fetchSel').value);
    if (picked) q += '&lat=' + picked.lat + '&lon=' + picked.lon + '&place=' + encodeURIComponent(picked.place);
    $('wxSave').disabled = true;
    api(q, 'POST').then(function(s){
      S = s; picked = null; renderPicked(); renderSettings();
      setMsg('wxMsg', 'Saved. The Weather page uses this from now on.', 'ok');
      var lines = [];
      if (S.location.set) lines.push('Location: ' + S.location.place);
      lines.push('Weather updates every ' + fmtInterval(S.fetch.min) + '.');
      lines.push('The device fetches the weather after you leave this mode.');
      showPopup('Saved', lines.join('\n'));
    }).catch(function(e){
      setMsg('wxMsg', e.status === 400 ? 'The device rejected these values.' : 'Could not reach the device.', 'bad');
    }).then(function(){ $('wxSave').disabled = false; });
  };

  $('wxPrev').onclick = function(){
    var loc = picked || (S && S.location.set ? S.location : null);
    if (!loc){ setMsg('wxMsg', 'Choose a location first.', 'bad'); return; }
    setMsg('wxMsg', 'Asking Open-Meteo\u2026', 'na');
    fetch('https://api.open-meteo.com/v1/forecast?latitude=' + loc.lat + '&longitude=' + loc.lon +
          '&current=temperature_2m,weather_code,wind_speed_10m&timezone=auto', {cache:'no-store'})
      .then(function(r){ return r.json(); })
      .then(function(j){
        var c = j.current;
        setMsg('wxMsg', loc.place + ': ' + c.temperature_2m + ' \u00b0C, ' + (WMO[c.weather_code] || 'unknown') +
               ', wind ' + Math.round(c.wind_speed_10m) + ' km/h', 'ok');
      })
      .catch(function(){ setMsg('wxMsg', "Couldn't reach Open-Meteo (does this phone have mobile data?).", 'bad'); });
  };

  // ---------- Wi-Fi tab ----------
  function renderScan(j){
    var list = $('scanList');
    clear(list);
    if (!j.done){ $('scanMeta').textContent = 'No scan available.'; return; }
    j.networks.forEach(function(n){
      var it = document.createElement('div');
      it.className = 'item';
      var bars = n.rssi >= -55 ? 4 : n.rssi >= -65 ? 3 : n.rssi >= -75 ? 2 : 1;
      var sig = document.createElement('span'); sig.className = 'sig s' + bars;
      sig.innerHTML = '<i></i><i></i><i></i><i></i>';
      var nm = document.createElement('span'); nm.className = 'nm'; nm.textContent = n.ssid;
      var tag = document.createElement('small'); tag.textContent = n.open ? 'open' : '\uD83D\uDD12';
      it.appendChild(sig); it.appendChild(nm); it.appendChild(tag);
      it.onclick = function(){
        var all = list.getElementsByClassName('item');
        for (var k = 0; k < all.length; k++) all[k].className = 'item';
        it.className = 'item sel';
        $('wSsid').value = n.ssid; $('wPass').value = '';
        renderSettings();
        if (!n.open) $('wPass').focus();
      };
      list.appendChild(it);
    });
    $('scanMeta').textContent = j.networks.length
      ? j.networks.length + ' networks, scanned ' + (j.ageSec < 90 ? j.ageSec + ' s' : Math.round(j.ageSec/60) + ' min') + ' before this page opened. Tap one to select it.'
      : 'The device found no networks.';
  }

  $('wShow').onclick = function(){
    var p = $('wPass'), show = p.type === 'password';
    p.type = show ? 'text' : 'password';
    this.textContent = show ? 'Hide' : 'Show';
  };
  $('wSsid').oninput = renderSettings;

  $('wSave').onclick = function(){
    var ssid = $('wSsid').value, pass = $('wPass').value;
    var bytes = (typeof TextEncoder !== 'undefined') ? new TextEncoder().encode(ssid).length : ssid.length;
    if (!ssid || bytes > 32){ setMsg('wMsg', 'Enter the network name (up to 32 characters).', 'bad'); return; }
    if (pass && (pass.length < 8 || pass.length > 63)){ setMsg('wMsg', 'A Wi-Fi password has 8 to 63 characters.', 'bad'); return; }
    var q = '/api/settings?ssid=' + encodeURIComponent(ssid) + (pass ? '&pass=' + encodeURIComponent(pass) : '');
    $('wSave').disabled = true;
    api(q, 'POST').then(function(s){
      S = s; $('wPass').value = ''; renderSettings();
      setMsg('wMsg', 'Saved. Password stays on the device and is never shown again.', 'ok');
      showPopup('Saved', 'Home network: ' + S.wifi.ssid + '\n' + (S.wifi.passSet ? 'Password saved on the device.' : 'No password.') +
                '\nThe device connects when you leave this mode.');
    }).catch(function(e){
      setMsg('wMsg', e.status === 400 ? 'The device rejected these values.' : 'Could not reach the device.', 'bad');
    }).then(function(){ $('wSave').disabled = false; });
  };

  $('wForget').onclick = function(){
    api('/api/settings?forgetWifi=1', 'POST').then(function(s){
      S = s; $('wSsid').value = ''; $('wPass').value = ''; renderSettings();
      setMsg('wMsg', 'Home network removed.', 'ok');
      showPopup('Removed', 'The home network was removed from the device.');
    }).catch(function(){ setMsg('wMsg', 'Could not reach the device.', 'bad'); });
  };

  // ---------- Tests (diagnostics) ----------
  function tfetch(url, opts, ms){
    var c = new AbortController();
    var t = setTimeout(function(){ c.abort(); }, ms || 7000);
    opts = opts || {}; opts.signal = c.signal; opts.cache = 'no-store';
    return fetch(url, opts).then(function(r){ clearTimeout(t); return r; },
                                 function(e){ clearTimeout(t); throw e; });
  }
  function runDiag(){
    var list = $('diagList'), text = [];
    list.innerHTML = '';
    $('diagBtn').disabled = true;
    function add(cls, mark, label, detail){
      var li = document.createElement('li');
      li.innerHTML = '<b class="' + cls + '">' + mark + '</b> ';
      li.appendChild(document.createTextNode(label + ': ' + detail));
      list.appendChild(li);
      text.push(mark + ' ' + label + ': ' + detail);
    }
    text.push('PaperColor diagnostics');
    text.push('Page: ' + location.href);
    text.push('Browser: ' + navigator.userAgent);

    add(window.isSecureContext ? 'ok' : 'bad', window.isSecureContext ? '\u2713' : '\u2717',
        'Secure context (HTTPS)', window.isSecureContext ? 'yes' : 'no (browsers block geolocation on plain http pages)');

    var geo = new Promise(function(done){
      if (!('geolocation' in navigator)){ add('bad','\u2717','Geolocation','not available in this browser'); return done(); }
      navigator.geolocation.getCurrentPosition(function(p){
        add('ok','\u2713','Geolocation','works: ' + p.coords.latitude.toFixed(3) + ', ' + p.coords.longitude.toFixed(3)); done();
      }, function(e){
        add('bad','\u2717','Geolocation','refused (code ' + e.code + ': ' + e.message + ')'); done();
      }, {timeout: 8000, maximumAge: 60000});
    });

    var internet = false;
    geo.then(function(){
      return tfetch('https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m', {mode:'cors'})
        .then(function(r){ return r.json(); })
        .then(function(j){
          internet = true;
          add('ok','\u2713','Open-Meteo weather API','reachable, browser access allowed (' + j.current.temperature_2m + ' \u00b0C in Berlin)');
        }, function(e){
          return tfetch('https://api.open-meteo.com/', {mode:'no-cors'}).then(function(){
            internet = true;
            add('bad','\u2717','Open-Meteo weather API','reachable, but the browser blocked the answer (CORS)');
          }, function(){
            add('bad','\u2717','Open-Meteo weather API','not reachable (' + e.name + '): this phone has no internet route on this Wi-Fi');
          });
        });
    }).then(function(){
      if (!internet){ add('na','\u2013','Open-Meteo geocoding (city search)','skipped, no internet'); return; }
      return tfetch('https://geocoding-api.open-meteo.com/v1/search?name=Amsterdam&count=1', {mode:'cors'})
        .then(function(r){ return r.json(); })
        .then(function(j){ add('ok','\u2713','Open-Meteo geocoding (city search)','works (' + (j.results && j.results[0] ? j.results[0].name + ', ' + j.results[0].country : 'no result') + ')'); },
              function(e){ add('bad','\u2717','Open-Meteo geocoding (city search)','failed (' + e.name + ')'); });
    }).then(function(){
      $('diagText').textContent = text.join('\n');
      $('diagBox').hidden = false;
      $('diagBtn').disabled = false;
    });
  }
  $('diagBtn').onclick = runDiag;

  function runSdTest(){
    var list = $('sdList');
    clear(list);
    $('sdBtn').disabled = true;
    function add(cls, mark, text){
      var li = document.createElement('li');
      li.innerHTML = '<b class="' + cls + '">' + mark + '</b> ';
      li.appendChild(document.createTextNode(text));
      list.appendChild(li);
    }
    api('/api/sdtest', 'POST').then(function(r){
      if (r.ok){
        add('ok', '\u2713', 'Card detected: ' + (r.type ? r.type + ', ' : '') + fmtSize(r.mb));
        add('ok', '\u2713', 'Write: ' + r.writeKBps + ' KB/s');
        add('ok', '\u2713', 'Read: ' + r.readKBps + ' KB/s');
        add('ok', '\u2713', 'Data read back matches, test file removed');
      } else {
        add('bad', '\u2717', r.error || 'Test failed');
        if (r.detected) add('na', '\u2013', 'A card is inserted but could not be used');
      }
      poll();
    }).catch(function(){ add('bad', '\u2717', 'Could not reach the device'); })
      .then(function(){ $('sdBtn').disabled = false; });
  }
  $('sdBtn').onclick = runSdTest;

  // ---------- adding and editing pictures ----------
  var ed = null;                                  // the open editor, or null
  var ED_ORIGINAL_MAX = 32 * 1024 * 1024;         // what the card accepts as an original
  function edMsg(t, c){ setMsg('edMsg', t, c || ''); }
  function readHead(blob, n){                     // Promise: the first n bytes of a file
    return new Promise(function(resolve, reject){
      var r = new FileReader();
      r.onload = function(){ resolve(new Uint8Array(r.result)); };
      r.onerror = function(){ reject(r.error); };
      r.readAsArrayBuffer(blob.slice(0, n));
    });
  }
  function fetchBlob(url){ return fetch(url).then(function(r){ if (!r.ok) throw new Error('HTTP ' + r.status); return r.blob(); }); }

  // Finished (saved, or the picture was stored but not its original): the editor goes away, and if the person is still looking at the
  // Upload Image tab the Image Gallery is shown, where the picture now is, with the result.
  function edDone(text, cls){
    edClose();
    if (curTab === 'upload') showTab('gallery');
    galSay(text, cls);
  }
  function edShow(which){ $('edPick').hidden = which !== 'pick'; $('edBox').hidden = which !== 'editor'; edMsg('', ''); }

  // ----- opening a photo -----
  function decodeImage(blob){                     // Promise: {img, w, h, close}
    function viaBitmap(){
      if (!window.createImageBitmap) return Promise.reject(new Error('no bitmaps'));
      return createImageBitmap(blob, { imageOrientation: 'from-image' }).catch(function(){ return createImageBitmap(blob); }).then(function(bm){
        return { img: bm, w: bm.width, h: bm.height, close: function(){ try { bm.close(); } catch(e){} } };
      });
    }
    function viaImg(){
      return new Promise(function(resolve, reject){
        var url, im = new Image();
        im.onload = function(){ try { URL.revokeObjectURL(url); } catch(e){} resolve({ img: im, w: im.naturalWidth, h: im.naturalHeight, close: function(){} }); };
        im.onerror = function(){ try { URL.revokeObjectURL(url); } catch(e){} reject(new Error('cannot decode')); };
        try { url = URL.createObjectURL(blob); im.src = url; } catch(e){ reject(e); }
      });
    }
    return viaBitmap().catch(viaImg);
  }
  function newCanvas(w, h){ var c = document.createElement('canvas'); c.width = w; c.height = h; return c; }
  function drawOnce(src, w, h){ var c = newCanvas(w, h), g = c.getContext('2d'); g.imageSmoothingQuality = 'high'; g.drawImage(src, 0, 0, w, h); return c; }
  // Shrinks a picture by repeated halving (much smoother than one big step). The first step never makes a canvas bigger than 4096 px.
  function scaleDown(src, w, h, tw, th){
    var cur = src, cw = w, ch = h, k, nw, nh;
    if (Math.max(cw, ch) > 4096){ k = 4096 / Math.max(cw, ch); nw = Math.max(tw, Math.round(cw * k)); nh = Math.max(th, Math.round(ch * k)); cur = drawOnce(cur, nw, nh); cw = nw; ch = nh; }
    while (cw / 2 >= tw && ch / 2 >= th){ nw = Math.ceil(cw / 2); nh = Math.ceil(ch / 2); cur = drawOnce(cur, nw, nh); cw = nw; ch = nh; }
    if (cw > tw || ch > th){ cur = drawOnce(cur, tw, th); cw = tw; ch = th; }      // the last step is less than a halving: exactly the size asked for
    return { c: cur, w: cw, h: ch };
  }

  $('edFile').onchange = function(){
    var file = this.files && this.files[0];
    try { this.value = ''; } catch(e){}
    if (file) edLoadFile(file);
  };
  function edLoadFile(file){
    edMsg('Opening ' + file.name + '\u2026', '');
    readHead(file, 24).then(function(head){
      var kind = PCImg.kindOfFile(head, file.name);
      if (kind === 'raw'){ edMsg("Raw photos aren't supported. Use a JPEG, PNG or TIFF.", 'bad'); return; }
      if (!kind){ edMsg("That doesn't look like a JPEG, PNG or TIFF photo.", 'bad'); return; }
      return decodeImage(file).then(function(img){
        edOpen({ mode: 'new', file: file, kind: kind, img: img, name: PCImg.suggestName(file.name) });
      }, function(){
        edMsg(kind === 'tiff' ? "This browser can't open TIFF files (Safari can). Save the photo as a JPEG instead." : 'This browser could not open that photo.', 'bad');
      });
    }, function(){ edMsg('Could not read that file.', 'bad'); });
  }

  function edOpen(o){
    edClose(true);
    var w = o.img.w, h = o.img.h;
    ed = { mode: o.mode, id: o.id || null, file: o.file || null, kind: o.kind || null, full: o.img, work: null,
           st: o.st || PCImg.sanitize({ orient: h > w ? 'port' : 'land' }), name: o.name || '', busy: false, ptr: {}, timer: null };
    if (Math.max(w, h) > 2400){ var k = 2400 / Math.max(w, h); ed.work = scaleDown(o.img.img, w, h, Math.round(w * k), Math.round(h * k)).c; }   // a smaller copy keeps dragging smooth
    edLock(false);                                // (a save that has just finished leaves the controls locked)
    edShow('editor');
    $('edInfo').textContent = o.mode === 'edit' ? 'Editing ' + o.id : 'New picture from ' + (o.file ? o.file.name : 'a photo');
    $('edName').readOnly = o.mode === 'edit';
    edMsg(o.note || '', '');
    edClamp(); edSync(); edSchedule();
  }
  function edClose(keepOpen){
    if (!ed) return;
    clearTimeout(ed.timer);
    try { ed.full.close(); } catch(e){}
    ed = null;
    edLock(false);
    if (!keepOpen) edShow('pick');
  }

  // ----- the picture on screen -----
  function edClamp(){ var v = PCImg.view(ed.st, ed.full.w, ed.full.h); ed.st.cx = Math.round(v.cx); ed.st.cy = Math.round(v.cy); }
  function edSync(){
    var st = ed.st, on = function(id, yes){ $(id).className = 'seg' + (yes ? ' on' : ''); $(id).setAttribute('aria-pressed', yes ? 'true' : 'false'); };
    on('edLand', st.orient === 'land'); on('edPort', st.orient === 'port'); on('edFill', st.mode === 'fill'); on('edFit', st.mode === 'fit');
    $('edBorder').hidden = st.mode !== 'fit'; $('edBorder').value = st.border;
    $('edZoom').value = Math.round(st.zoom * 100); $('edZoomV').textContent = Math.round(st.zoom * 100) + '%';
    $('edB').value = st.b; $('edBV').textContent = st.b; $('edC').value = st.c; $('edCV').textContent = st.c; $('edS').value = st.s; $('edSV').textContent = st.s;
    $('edW').value = st.w; $('edWV').textContent = st.w; $('edT').value = st.t; $('edTV').textContent = st.t;
    $('edDither').value = st.dither; $('edName').value = ed.name;
  }
  function edFrame(g, v, src, sw, sh, st){        // the photo placed in the frame
    g.fillStyle = st.border === 'black' ? '#000000' : '#ffffff';
    g.fillRect(0, 0, v.W, v.H);
    g.save();
    g.translate(v.W / 2 + v.cx, v.H / 2 + v.cy);
    g.rotate(v.rot * Math.PI / 2);
    g.scale(v.scale, v.scale);
    g.imageSmoothingQuality = 'high';
    g.drawImage(src, -sw / 2, -sh / 2, sw, sh);
    g.restore();
  }
  function edDraw(dithered){                      // dithered: show it the way the panel will (slower), else just the photo with the tone settings
    if (!ed) return;
    var st = ed.st, v = PCImg.view(st, ed.full.w, ed.full.h), cv = $('edCanvas');
    if (cv.width !== v.W || cv.height !== v.H){ cv.width = v.W; cv.height = v.H; }
    var g = cv.getContext('2d', { willReadFrequently: true });
    edFrame(g, v, (ed.work && v.scale < 0.5) ? ed.work : ed.full.img, ed.full.w, ed.full.h, st);
    if (st.b || st.c || st.s || st.w || st.t || dithered){
      var im = g.getImageData(0, 0, v.W, v.H);
      PCImg.adjust(im.data, v.W * v.H, st);
      if (dithered){ PCImg.quantize(st.dither, im.data, v.W, v.H); PCImg.toInk(im.data, v.W, v.H); }
      g.putImageData(im, 0, 0);
    }
  }
  function edSchedule(){ if (!ed) return; clearTimeout(ed.timer); edDraw(false); ed.timer = setTimeout(function(){ edDraw(true); }, 220); }
  function edSet(ch){
    if (!ed || ed.busy) return;
    for (var k in ch) if (ch.hasOwnProperty(k)) ed.st[k] = ch[k];
    ed.st = PCImg.sanitize(ed.st);
    edClamp(); edSync(); edSchedule();
  }

  // ----- the controls -----
  $('edLand').onclick = function(){ edSet({ orient: 'land' }); };
  $('edPort').onclick = function(){ edSet({ orient: 'port' }); };
  $('edRot').onclick = function(){ if (ed) edSet({ rot: (ed.st.rot + 1) & 3 }); };
  $('edFill').onclick = function(){ edSet({ mode: 'fill' }); };
  $('edFit').onclick = function(){ edSet({ mode: 'fit' }); };
  $('edBorder').onchange = function(){ edSet({ border: this.value }); };
  $('edZoom').oninput = function(){ edSet({ zoom: this.value / 100 }); };
  $('edB').oninput = function(){ edSet({ b: +this.value }); };
  $('edC').oninput = function(){ edSet({ c: +this.value }); };
  $('edS').oninput = function(){ edSet({ s: +this.value }); };
  $('edW').oninput = function(){ edSet({ w: +this.value }); };
  $('edT').oninput = function(){ edSet({ t: +this.value }); };
  $('edDither').onchange = function(){ edSet({ dither: this.value }); };
  $('edName').oninput = function(){ if (ed && ed.mode === 'new') ed.name = this.value; };
  $('edReset').onclick = function(){ if (ed && !ed.busy){ ed.st = PCImg.sanitize({ orient: ed.full.h > ed.full.w ? 'port' : 'land' }); edClamp(); edSync(); edSchedule(); } };
  $('edCancel').onclick = function(){
    if (ed && ed.busy) return;
    var editing = !!ed && ed.mode === 'edit';
    edClose();
    if (editing) showTab('gallery');                   // (back where the person came from; a new photo just goes back to the choice of a photo)
  };

  // drag to move, pinch or wheel to zoom
  var edCv = $('edCanvas');
  function edFactor(){ var r = edCv.getBoundingClientRect(), w = (r && r.width) || edCv.clientWidth || edCv.width; return edCv.width / w; }
  edCv.addEventListener('pointerdown', function(e){
    if (!ed || ed.busy) return;
    ed.ptr[e.pointerId] = { x: e.clientX, y: e.clientY };
    try { edCv.setPointerCapture(e.pointerId); } catch(x){}
    if (e.preventDefault) e.preventDefault();
  });
  edCv.addEventListener('pointermove', function(e){
    if (!ed || ed.busy || !ed.ptr[e.pointerId]) return;
    var ids = Object.keys(ed.ptr), prev = ed.ptr[e.pointerId];
    if (ids.length === 1){
      var f = edFactor();
      edSet({ cx: ed.st.cx + (e.clientX - prev.x) * f, cy: ed.st.cy + (e.clientY - prev.y) * f });
    } else if (ids.length === 2){
      var other = ed.ptr[String(ids[0]) === String(e.pointerId) ? ids[1] : ids[0]];
      var d0 = Math.sqrt(Math.pow(prev.x - other.x, 2) + Math.pow(prev.y - other.y, 2)), d1 = Math.sqrt(Math.pow(e.clientX - other.x, 2) + Math.pow(e.clientY - other.y, 2));
      if (d0 > 0 && d1 > 0) edSet({ zoom: ed.st.zoom * d1 / d0 });
    }
    if (ed) ed.ptr[e.pointerId] = { x: e.clientX, y: e.clientY };
  });
  function edLift(e){ if (ed) delete ed.ptr[e.pointerId]; }
  edCv.addEventListener('pointerup', edLift);
  edCv.addEventListener('pointercancel', edLift);
  edCv.addEventListener('wheel', function(e){
    if (!ed || ed.busy) return;
    if (e.preventDefault) e.preventDefault();
    edSet({ zoom: ed.st.zoom * (e.deltaY < 0 ? 1.1 : 1 / 1.1) });
  }, { passive: false });

  // ----- making the files -----
  function toBlobP(canvas){ return new Promise(function(resolve, reject){ canvas.toBlob(function(b){ if (b) resolve(b); else reject(new Error('export failed')); }, 'image/png'); }); }
  function edExportSource(v){                     // the full photo when it is shown at half size or more, else a smoothly shrunk copy
    if (v.scale >= 0.5) return ed.full.img;
    var base = ed.work || ed.full.img, bw = ed.work ? ed.work.width : ed.full.w, bh = ed.work ? ed.work.height : ed.full.h;
    var tw = Math.round(ed.full.w * v.scale * 2), th = Math.round(ed.full.h * v.scale * 2);
    return (bw <= tw || bh <= th) ? base : scaleDown(base, bw, bh, tw, th).c;
  }
  function edThumb(c, v){                         // 150x100 (or 100x150) from the finished picture; null if the browser can't
    return new Promise(function(resolve){
      try {
        var t = newCanvas(v.W > v.H ? 150 : 100, v.W > v.H ? 100 : 150), g = t.getContext('2d');
        g.imageSmoothingQuality = 'high';
        g.drawImage(c, 0, 0, t.width, t.height);
        t.toBlob(function(b){ resolve(b || null); }, 'image/png');
      } catch(e){ resolve(null); }
    });
  }
  function edBuild(){                             // Promise: {png, thumb}. "Panel" keeps the colours as they are (the panel driver dithers them); the others are reduced here.
    var st = ed.st, v = PCImg.view(st, ed.full.w, ed.full.h), c = newCanvas(v.W, v.H), g = c.getContext('2d', { willReadFrequently: true });
    edFrame(g, v, edExportSource(v), ed.full.w, ed.full.h, st);
    var im = g.getImageData(0, 0, v.W, v.H);
    PCImg.adjust(im.data, v.W * v.H, st);
    if (st.dither !== 'panel') PCImg.quantize(st.dither, im.data, v.W, v.H);
    g.putImageData(im, 0, 0);
    return toBlobP(c).then(function(blob){
      return readHead(blob, blob.size).then(function(bytes){
        var png = new Blob([PCImg.pngInsertText(bytes, [['pcedit', PCImg.encodeSettings(st)]])], { type: 'image/png' });   // how it was edited travels inside the picture
        return edThumb(c, v).then(function(thumb){ return { png: png, thumb: thumb }; });
      });
    });
  }

  // ----- saving -----
  function xhrUpload(path, form, onProgress){    // Promise: the device's answer (the new picture list)
    return new Promise(function(resolve, reject){
      var x = new XMLHttpRequest();
      x.open('POST', path);
      x.upload.onprogress = function(e){ if (e.lengthComputable) onProgress(e.loaded / e.total); };
      x.onload = function(){
        if (x.status >= 200 && x.status < 300){
          try { resolve(JSON.parse(x.responseText)); } catch(e){ reject({status: 0, text: 'The device sent an answer that could not be read.'}); }
        } else reject({status: x.status, text: x.responseText || ''});
      };
      x.onerror = function(){ reject({status: 0, text: ''}); };
      x.send(form);
    });
  }
  function upExplain(e){
    if (e.status === 0) return e.text || 'Could not reach the device.';
    return e.text ? e.text : 'HTTP ' + e.status;
  }
  function edLock(b){                             // the controls and the progress bar (they belong to the page, not to one editor)
    var ids = ['edLand', 'edPort', 'edRot', 'edFill', 'edFit', 'edBorder', 'edZoom', 'edB', 'edC', 'edS', 'edW', 'edT', 'edDither', 'edName', 'edSave', 'edReset', 'edCancel'];
    for (var i = 0; i < ids.length; i++) $(ids[i]).disabled = b;
    $('edProg').hidden = !b;
  }
  function edBusy(b){ ed.busy = b; edLock(b); }
  function edSave(){
    if (!ed || ed.busy) return;
    var cur = ed, name = cur.mode === 'edit' ? cur.id : cur.name, prog = $('edProg');
    if (!/^[A-Za-z0-9_\-]{1,40}$/.test(name)){ edMsg('Name: letters, digits, _ and - only (40 at most).', 'bad'); return; }
    var hasFile = cur.mode === 'new' && !!cur.file, withOriginal = hasFile && cur.file.size <= ED_ORIGINAL_MAX, tooBig = hasFile && !withOriginal, added = null;
    edBusy(true); prog.value = 0;
    edMsg('Preparing the picture\u2026', '');
    edBuild().then(function(out){
      var f = new FormData();
      f.append('png', out.png, 'picture.png');
      if (out.thumb) f.append('thumb', out.thumb, 'thumb.png');
      edMsg('Uploading the picture\u2026', '');
      return xhrUpload('/api/images/upload?id=' + encodeURIComponent(name) + '&replace=' + (cur.mode === 'edit' ? 1 : 0), f, function(p){ prog.value = Math.round(p * (withOriginal ? 30 : 100)); });
    }).then(function(list){
      added = list.added; renderGallery(list);
      if (!withOriginal) return null;
      edMsg('Uploading the original\u2026', '');
      var f2 = new FormData();
      f2.append('master', cur.file, 'original.' + (cur.kind === 'jpeg' ? 'jpg' : cur.kind === 'tiff' ? 'tif' : 'png'));
      return xhrUpload('/api/images/master?id=' + encodeURIComponent(added), f2, function(p){ prog.value = 30 + Math.round(p * 70); }).then(renderGallery);
    }).then(function(){
      var text = (cur.mode === 'edit' ? 'Saved ' : 'Added ') + added + (withOriginal ? ' with its original.' : '.');
      if (tooBig) text += ' The original is larger than 32 MB, so it was not kept on the card.';
      edDone(text, 'ok');
    }, function(e){
      var t = (e && e.status !== undefined) ? upExplain(e) : 'Could not prepare the picture.';     // (an answer from the device, or something went wrong here)
      if (added) edDone('The picture was saved as ' + added + ', but the original could not be stored: ' + t, 'bad');
      else edMsg(t, 'bad');
    }).then(function(){ if (ed === cur) edBusy(false); });
  }
  $('edSave').onclick = edSave;

  // ----- editing a picture that is already on the card -----
  function galEdit(it){
    showTab('upload');
    if (ed && ed.busy) return;                           // (a picture is being saved right now: the person is taken there, nothing else is started)
    edShow('pick');
    edMsg('Opening ' + it.id + '\u2026', '');
    fetchBlob(galUrl(it, 'image')).then(function(pic){
      return readHead(pic, pic.size).then(function(bytes){
        // the notes inside the picture say how it was edited; they only make sense together with the original
        var st = it.master ? PCImg.decodeSettings(PCImg.pngReadText(bytes).pcedit || '') : null;
        if (!st) st = PCImg.sanitize({ orient: it.h > it.w ? 'port' : 'land' });
        return (it.master ? fetchBlob(galUrl(it, 'master')) : Promise.resolve(pic)).then(function(src){
          return decodeImage(src).then(function(img){
            edOpen({ mode: 'edit', id: it.id, img: img, name: it.id, st: st, note: it.master ? '' : 'This picture has no original on the card, so it is edited as it is.' });
          });
        });
      });
    }).catch(function(){ edMsg(it.master >= 4 ? "This browser can't open TIFF files (Safari can)." : 'Could not open that picture for editing.', 'bad'); });
  }

  // ---------- pictures ----------
  var gal = null, galBusy = false, cfResolve = null;
  var galStatus = false;      // does galMsg hold a status line written by renderGallery (no card ...), as opposed to the result of something the person did?
  function galSay(text, cls){ galStatus = false; setMsg('galMsg', text, cls); }
  function fmtBytes(b){ return b >= 1048576 ? (b / 1048576).toFixed(1) + ' MB' : Math.max(1, Math.round(b / 1024)) + ' KB'; }
  function galUrl(it, kind){ return '/api/images/file?id=' + encodeURIComponent(it.id) + '&kind=' + kind + '&v=' + it.bytes; }
  function mkTag(text, cls){ var t = document.createElement('span'); t.className = 'tag ' + (cls || ''); t.textContent = text; return t; }
  function mkBtn(text, cls, fn, disabled){
    var b = document.createElement('button');
    b.type = 'button'; b.textContent = text; if (cls) b.className = cls;
    b.disabled = !!disabled; b.onclick = fn;
    return b;
  }

  function galRow(it, idx, n){
    var row = document.createElement('div');
    row.className = 'gi' + (it.on ? '' : ' off');
    var img = document.createElement('img');
    img.className = 'th'; img.alt = it.id; img.loading = 'lazy';
    img.onerror = function(){ if (it.thumb && !img.fallback){ img.fallback = true; img.src = galUrl(it, 'image'); } };
    img.src = galUrl(it, it.thumb ? 'thumb' : 'image');
    img.onclick = function(){ openPreview(it); };
    var m = document.createElement('div'); m.className = 'gm';
    var nm = document.createElement('div'); nm.className = 'gn'; nm.textContent = it.id;
    var d = document.createElement('div'); d.className = 'gd';
    if (!it.ok){ d.appendChild(mkTag('not a readable PNG', 'bad')); }
    else {
      d.appendChild(document.createTextNode(it.w + '\u00d7' + it.h));
      if (!it.fits) d.appendChild(mkTag('wrong size: needs 600\u00d7400', 'bad'));
    }
    d.appendChild(document.createTextNode(' \u00b7 ' + fmtBytes(it.bytes)));
    if (it.master) d.appendChild(mkTag('original', 'ok'));
    var a = document.createElement('div'); a.className = 'ga';
    a.appendChild(mkBtn(it.on ? 'In slideshow' : 'Hidden', it.on ? 'on' : '', function(){ galDo('/api/images/enable?id=' + encodeURIComponent(it.id) + '&on=' + (it.on ? 0 : 1)); }));
    if (it.ok || it.master) a.appendChild(mkBtn('Edit', '', function(){ galEdit(it); }));
    a.appendChild(mkBtn('Up', '', function(){ galMove(idx, -1); }, idx === 0));
    a.appendChild(mkBtn('Down', '', function(){ galMove(idx, 1); }, idx === n - 1));
    a.appendChild(mkBtn('Delete', 'del', function(){ galDelete(it); }));
    m.appendChild(nm); m.appendChild(d); m.appendChild(a);
    row.appendChild(img); row.appendChild(m);
    return row;
  }

  function renderGallery(j){
    gal = j;
    var box = $('galList'); clear(box);
    var sum = '', msg = '', cls = 'na';
    if (j.sd === 'absent'){ msg = 'No SD card. Put a card in the device to see its pictures.'; }
    else if (j.sd === 'error'){ msg = "The SD card can't be used: " + j.error; cls = 'bad'; }
    else {
      sum = j.total + (j.total === 1 ? ' picture, ' : ' pictures, ') + j.enabled + ' in the slideshow';
      if (j.ignored) sum += ' \u00b7 ' + j.ignored + ' other file' + (j.ignored === 1 ? '' : 's') + ' ignored';
      if (j.total === 0) msg = 'No pictures yet. Add one in the Upload Image tab.';
    }
    $('galSum').textContent = sum || '\u00a0';
    if (msg){ setMsg('galMsg', msg, cls); galStatus = true; }
    else if (galStatus){ setMsg('galMsg', '', ''); galStatus = false; }       // (only a status line is taken away: the result of what the person did stays)
    for (var i = 0; i < j.items.length; i++) box.appendChild(galRow(j.items[i], i, j.items.length));
  }

  function loadGallery(){
    return api('/api/images').then(renderGallery, function(e){
      galSay('Could not read the pictures (' + (e.status ? 'HTTP ' + e.status : 'no connection') + ').', 'bad');
    });
  }

  // One change at a time: the device answers every change with the new list.
  function galDo(path){
    if (galBusy) return;
    galBusy = true;
    galSay('', '');
    api(path, 'POST').then(renderGallery, function(e){
      var text = e.status === 409 ? 'No SD card.' : (e.status ? "That didn't work (HTTP " + e.status + ')' : 'Could not reach the device.');
      var again = e.status ? loadGallery() : Promise.resolve();      // show what the card really holds
      return again.then(function(){ galSay(text, 'bad'); });
    }).then(function(){ galBusy = false; });
  }

  function galMove(idx, dir){
    if (!gal) return;
    var ids = [];
    for (var i = 0; i < gal.items.length; i++) ids.push(gal.items[i].id);
    var j = idx + dir;
    if (j < 0 || j >= ids.length) return;
    var t = ids[idx]; ids[idx] = ids[j]; ids[j] = t;
    galDo('/api/images/order?ids=' + ids.join(','));
  }

  function galDelete(it){
    confirmBox('Delete ' + it.id + '?',
               'The picture' + (it.master ? ', its original' : '') + ' and its preview will be removed from the card. This cannot be undone.',
               'Delete').then(function(yes){
      if (yes) galDo('/api/images/delete?id=' + encodeURIComponent(it.id));
    });
  }
  $('galRescan').onclick = function(){ galDo('/api/images/rescan'); };

  function closeConfirm(result){
    if ($('cf').hidden) return;
    $('cf').hidden = true;
    var r = cfResolve; cfResolve = null;
    if (r) r(!!result);
  }
  function confirmBox(title, text, okLabel){
    return new Promise(function(resolve){
      if (cfResolve) cfResolve(false);
      cfResolve = resolve;
      $('cfTitle').textContent = title;
      $('cfText').textContent = text;
      $('cfOk').textContent = okLabel || 'OK';
      $('cf').hidden = false;
      $('cfCancel').focus();
    });
  }
  $('cfOk').onclick = function(){ closeConfirm(true); };
  $('cfCancel').onclick = function(){ closeConfirm(false); };
  $('cf').onclick = function(e){ if (e.target === this) closeConfirm(false); };

  function openPreview(it){
    $('pvTitle').textContent = it.id + (it.ok ? '  (' + it.w + '\u00d7' + it.h + ')' : '');
    $('pvImg').src = galUrl(it, 'image');
    var o = $('pvOrig');
    if (it.master){ o.href = galUrl(it, 'master'); o.hidden = false; } else { o.hidden = true; }
    $('pv').hidden = false;
    $('pvClose').focus();
  }
  function closePreview(){ $('pv').hidden = true; $('pvImg').removeAttribute('src'); }
  $('pvClose').onclick = closePreview;
  $('pv').onclick = function(e){ if (e.target === this) closePreview(); };

  // ---------- start ----------
  function poll(){
    api('/api/status').then(function(s){ applyStatus(s, false); }).catch(function(){});
  }
  showTab((location.hash || '').replace('#', ''));
  api('/api/status').then(function(s){ applyStatus(s, true); return syncTime(true); }).catch(function(){});
  api('/api/settings').then(function(s){ S = s; renderSettings(); }).catch(function(){});
  api('/api/wifi/scan').then(renderScan).catch(function(){});
  setInterval(tick, 1000); tick();
  setInterval(poll, 8000);
})();
</script>
</body>
</html>
)PCHTML";
