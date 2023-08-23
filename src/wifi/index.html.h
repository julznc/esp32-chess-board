static const char index_html[] = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Marius eBoard</title>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
body {
  font-family:Arial,Helvetica,sans-serif;
  justify-content: center; padding: 10px; background: #efefef;
}
.tabs {
  display: flex; flex-wrap: wrap;
  background: #e5e5e5; box-shadow: 0 48px 80px -32px rgba(0,0,0,0.3);
}
.tabs .tab-label {
  width: auto; padding: 10px 20px; cursor: pointer; font-weight: bold;
  color: #C4A484; background: #eee; transition: background 0.1s, color 0.1s;
}
.tabs .tab-label:hover { background: #ddd; }
.tabs .tab-label:active { background: #ccc; }
.tabs .panel { display: none; width: 100%; padding: 10px; background: #fff; order: 99; }
.tabs .tab-input { position: absolute; opacity: 0; }
.tabs .tab-input:focus + .tab-label { z-index: 1; }
.tabs .tab-input:checked + .tab-label { background: #fff; color: #6F4E37; }
.tabs .tab-input:checked + .tab-label + .panel { display: block; }

form label { display: inline-block; text-align: right; }
form input { margin: 2px auto;}

  </style>
</head>

<body>
  <h4 style="color: #6F4E37;">MariusLab Electronic Chessboard</h4>

  <div class="tabs">
    <input class="tab-input" name="tabs" type="radio" id="tab-1" checked="checked"/>
    <label class="tab-label" for="tab-1">Home</label>
    <div class="panel">
      <fieldset><legend>Info</legend> <a id="txt-username" target="_blank">...</a><br><span id="txt-ver">...</span><br></fieldset><br>
      <fieldset><legend>PGN</legend><span id="txt-pgn">...</span><br></fieldset>
    </div>

    <input class="tab-input" name="tabs" type="radio" id="tab-2"/>
    <label class="tab-label" for="tab-2">Lichess</label>
    <div class="panel">
      <form method="post" action="/lichess-token">
        <fieldset><legend>Access Token</legend>
          <input type="text" name="token" size="32" minlength="20" required><input type="submit" value="Set"><br><br>
          <a href="https://lichess.org/account/oauth/token/create?scopes[]=challenge:write&scopes[]=challenge:read&scopes[]=board:play" target="_blank">Generate a personal access token.</a>
        </fieldset>
      </form><br>
      <form method="post" action="/lichess-challenge">
        <fieldset><legend>Games</legend>
          <label style="width: 9ch;">Limit</label>&nbsp;<input type="number" name="clock-limit" min="10" max="90" size="5" required>&nbsp;<label>minutes</label><br>
          <label style="width: 9ch;">Increment</label>&nbsp;<input type="number" name="clock-increment" min="0" max="30" size="5" required>&nbsp;<label>seconds</label><br>
          <label style="width: 9ch;">Opponent</label>&nbsp;<input type="text" name="opponent" placeholder="(optional)"><br>
          <label style="width: 9ch;">Mode</label>&nbsp;
          <input type="radio" name="mode" id="rated" value="rated" disabled><label for="rated">Rated</label>
          <input type="radio" name="mode" id="casual" value="casual" disabled checked><label for="casual">Casual</label><br>
          <input type="submit" style="margin: 3px 11.5ch -2px; padding: 0 2ch;" value="Set">
        </fieldset>
      </form>
    </div>

    <input class="tab-input" name="tabs" type="radio" id="tab-3"/>
    <label class="tab-label" for="tab-3">Settings</label>
    <div class="panel">
      <form method="post" action="/wifi-cfg">
        <fieldset><legend>Configure Wi-Fi</legend>
          <label style="width: 8ch;">SSID</label>&nbsp;<input type="text" name="ssid" required><br>
          <label style="width: 8ch;">Password</label>&nbsp;<input type="text" name="passwd" required><br>
          <input type="submit" style="margin: 3px 10.5ch -2px; padding: 0 2ch;" value="Add">
        </fieldset>
      </form><br><br>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <fieldset><legend>FW Update</legend><input type='file' name='update' accept=".bin" required><input type='submit' value='Update'></fieldset>
      </form><br>
      <form method="post" action="/reset">
        <fieldset><legend>Reset</legend>
          <input type="submit" name="restart" value="Restart Application">&nbsp;&nbsp;
          <input type="submit" name="restore" value="Restore Factory Settings">
        </fieldset>
      </form><br>
    </div>
  </div>

  <script type="text/javascript">
(function() {
  const txt_username = document.getElementById("txt-username");
  const txt_ver = document.getElementById("txt-ver");
  const txt_pgn = document.getElementById("txt-pgn");
  fetch("/username").then(function(rsp) {
    rsp.text().then(function (txt) {
      if (txt.length >= 3) {
        txt_username.innerText = "@" + txt; txt_username.setAttribute("href", "https://lichess.org/@/" + txt);
      } else {
        txt_username.innerText = "unknown Lichess account"; txt_username.style.color = "#e00";
      }
    });
  }).catch(function(err){ console.log(err); });
  fetch("/version").then(function(rsp) {
    rsp.text().then(function (txt) { txt_ver.innerText = "FW v" + txt; });
  }).catch(function(err){ console.log(err); });
  fetch("/pgn").then(function(rsp) {
    rsp.text().then(function (txt) { txt_pgn.innerText = txt; });
  }).catch(function(err){ console.log(err); });
})();
  </script>
</body>
</html>
)rawliteral";
