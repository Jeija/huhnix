function api_action(action, cb) {
	var xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange = function() {
		if (xmlhttp.readyState == 4) {
			if (cb) cb(xmlhttp.responseText);
		}
	}
	xmlhttp.open("GET", action, true);
	xmlhttp.send();
}

function api_okmessage(res) {
	if (res == "ok") {
		alert("Aktion erfolgreich!");
	} else {
		alert("Fehler: " + res);
	}
}

function api_message(res) {
	alert(res);
}

function api_set_opentime(hours, minutes) {
	var xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange = function() {
		if (xmlhttp.readyState == 4) {
			if (xmlhttp.responseText == "ok") {
				alert("Aufmachzeit geändert!");
			} else {
				alert("Fehler: " + xmlhttp.responseText);
			}
		}
	}


	var url = "opentime_set?hours=" + hours + "&minutes=" + minutes;

	xmlhttp.open("POST", url, true);
	xmlhttp.send();
}

function set_opentime() {
	var hours = document.getElementById("opentime_hours").value;
	var minutes = document.getElementById("opentime_minutes").value;

	if (hours == "" || minutes == "" || isNaN(hours) || isNaN(minutes))
		alert("Ungültige Zeit!");
	else api_set_opentime(hours, minutes);
}
