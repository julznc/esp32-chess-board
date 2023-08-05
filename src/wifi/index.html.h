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
  color: #888; background: #eee; transition: background 0.1s, color 0.1s;
}
.tabs .tab-label:hover { background: #ddd; }
.tabs .tab-label:active { background: #ccc; }
.tabs .panel { display: none; width: 100%; padding: 10px; background: #fff; order: 99; }
.tabs .tab-input { position: absolute; opacity: 0; }
.tabs .tab-input:focus + .tab-label { z-index: 1; }
.tabs .tab-input:checked + .tab-label { background: #fff; color: #000; }
.tabs .tab-input:checked + .tab-label + .panel { display: block; }

  </style>
</head>

<body>
  <h4>Marius Electronic Chessboard</h4>

  <div class="tabs">
    <input class="tab-input" name="tabs" type="radio" id="tab-1" checked="checked"/>
    <label class="tab-label" for="tab-1">Home</label>
    <div class="panel">
      <p>FW v<span id="txt-ver">...</span></p>
    </div>

    <input class="tab-input" name="tabs" type="radio" id="tab-2"/>
    <label class="tab-label" for="tab-2">Settings</label>
    <div class="panel">
      <p>todo: configure</p><br>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <fieldset>
          <legend>FW Update</legend><br>
          <input type='file' name='update' accept=".bin">
          <input type='submit' value='Update'>
        </fieldset>
      </form>
    </div>
  </div>

  <script type="text/javascript">
(function() {
  const txt_ver = document.getElementById("txt-ver");
  fetch("/version").then(function(rsp) {
    rsp.text().then(function (txt) {
      txt_ver.innerText= txt;
    });
  }).catch(function(err){
    console.log(err);
  });
})();
  </script>
</body>
</html>
)rawliteral";
