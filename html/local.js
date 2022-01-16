"use strict";

var api = (function () {

	"use strict";

	var websocket;

	var led_lines = 1;

	var disconnectTimeout;

	function loading(on) {
		if (on)
			$("#loading").show();
		else
			$("#loading").hide();
	}

	function addSomeMoreLedLines(lines) {
		if (lines == led_lines)
			return;

		var la = $("#l0blocka");
		var lb = $("#l0blockb");
		var after = lb;

		for (var i = 1; i < lines; i++) {

			var nodename = "l" + i + "blocka";
			if ($("#" + nodename).length == 0) {
				var node = la.clone(true, true);
				node.attr("id", "l" + i + "blocka");
				var e = node.find("span");
				var text = e.text();
				e.text(text.substring(0, text.length - 1) + (i + 1));
				node.insertAfter(after);
				after = node;

				node = lb.clone(true, true);
				node.attr("id", "l" + i + "blockb");
				node.find("#l0sx").attr("id", "l" + i + "sx");
				node.find("#l0sy").attr("id", "l" + i + "sy");
				node.find("#l0ox").attr("id", "l" + i + "ox");
				node.find("#l0oy").attr("id", "l" + i + "oy");
				node.find("#l0s").attr("id", "l" + i + "s");
				node.find("#l0r").attr("id", "l" + i + "r");
				node.find("#l0op").attr("id", "l" + i + "op");
				node.insertAfter(after);
				after = node;
			} else {
				after = $("#l" + i + "blockb")[0];
			}
		}

		for (i = lines; i < led_lines; i++) {
			$("#l" + i + "blocka").remove();
			$("#l" + i + "blockb").remove();
		}
		led_lines = lines;
	}

	var playlist_lines = 1;

	function setPlaylistLines(lines) {

		console.log("Playlist lines " + lines);

		var first = $("#p0");
		var after = first[0];

		for (var i = 1; i < lines; i++) {

			var node = $("#p" + i);
			if (node.length == 0) {
				node = first.clone(true, true);
				node.attr("id", "p" + i);
				node.find("#p0duration").attr("id", "p" + i + "duration");
				node.find("#p0filename").attr("id", "p" + i + "filename");
				node.find("#p0mode").attr("id", "p" + i + "mode");
				node.insertAfter(after);
				after = node[0];
			} else {
				after = node[0];
			}
		}


		for (i = lines; i < playlist_lines; i++) {
			$("#p" + i).remove();
		}
		playlist_lines = lines;
	}

	function statusUpdate(status) {
		console.log("statusUpdate", status);
		$("#statusVersion").val(status.compile_git.split('\n')[0]);
		$("#statusFrom").val(status.compile_date);
		$("#statusMAC").val(status.MAC);
		$("#statusFree").val(status.free);
		$("#statusBootPartition").val(status.boot_partition);
		$("#statusRtpServer").val(status.server_state);
		$("#statusRtp").val(
			status.rtp_good + " / " + status.rtp_loss + " / "
			+ status.rtp_error);
		$("#statusMjpeg").val(
			status.mjpeg_good + " / " + status.mjpeg_loss + " / "
			+ status.mjpeg_error);
		$("#statusArtnet").val(
			status.artnet_good + " / " + status.artnet_loss + " / "
			+ status.artnet_error);
		$("#statusRtpLast").val(status.rtp_last);
		var sum = Number(status.led_on_time) + Number(status.led_top_too_slow)
			+ Number(status.led_bottom_too_slow);
		if (sum > 0) {
			var v1 = Math.round(1000. * Number(status.led_on_time) / sum) / 10.;
			var v2 = Math.round(1000. * Number(status.led_top_too_slow) / sum) / 10.;
			var v3 = Math.round(1000. * Number(status.led_bottom_too_slow) / sum) / 10.;

			$("#statusLed").val(v1 + "% good / " + v2 + "% decoding too slow / " + v3 + "% playback too slow");
		} else {
			$("#statusLed").val("");
		}

		var res = "";
		if (status.time >= 86400)
			res += Math.floor(status.time / 86400) + " d ";
		if (status.time >= 3600)
			res += (Math.floor(status.time / 3600) % 24) + " h ";
		if (status.time >= 60)
			res += (Math.floor(status.time / 60) % 60) + " m ";
		res += Math.floor(status.time % 60) + " s";
		$("#statusTime").val(res);

		$("#statusSTA").val(status.net_sta);
		$("#statusAP").val(status.net_ap);
		$("#statusEthernet").val(status.net_ethernet);
		var s = "";
		for (var i = 0; i < status.net_scan.length; i++)
			s += status.net_scan[i] + "\n";
		$("#statusScan").text(s);
		$("#tasks").text(status.tasks);
		$("#statusNTP").text(status.net_ntp);
		$("#statusGEOIP").text(status.net_geoip);
	}

	function configUpdate(status) {
		console.log("configUpdate", status);

		$("#statusRTPPort").val(status.rtp_port);
		$("#statusClientSSID").val(status.wifi_sta_ssid);
		$("#statusClientPassword").val(status.wifi_sta_password);
		$("#statusAPSSID").val(status.wifi_ap_ssid);
		$("#statusAPPassword").val(status.wifi_ap_password);
		$("#statusRegistrationServer").val(status.registration_server);

		$("#refresh_rate").val(status.refresh_rate);
		$("#prefix_leds").val(status.prefix_leds);
		$("#artnet_width").val(status.artnet_width);
		$("#artnet_universe_offset").val(status.artnet_universe_offset);

		addSomeMoreLedLines(status.channels);
		for (var i = 0; i < status.channels; i++) {
			$("#l" + i + "sx").val(status.leds[i].sx);
			$("#l" + i + "sy").val(status.leds[i].sy);
			$("#l" + i + "ox").val(status.leds[i].ox);
			$("#l" + i + "oy").val(status.leds[i].oy);
			$("#l" + i + "s").prop('selectedIndex', status.leds[i].s);
			$("#l" + i + "r").prop('selectedIndex', status.leds[i].r);
			$("#l" + i + "op").val(status.leds[i].op);
		}

		setPlaylistLines(status.playlist.length);
		for (i = 0; i < status.playlist.length; i++) {
			if (status.playlist[i].mode == 0) {
				$("#p" + i).hide();
			}
			else {
				$("#p" + i).show();
				$("#p" + i + "filename").text(status.playlist[i].filename);
				$("#p" + i + "duration").val(status.playlist[i].duration);
				$("#p" + i + "mode").val(status.playlist[i].mode);
			}
		}


		$("#colorContrast1").val(status.contrast);
		$("#colorLightness1").val(status.brightness);
		$("#colorRedContrast1").val(status.red_contrast);
		$("#colorRedLightness1").val(status.red_brightness);
		$("#colorGreenContrast1").val(status.green_contrast);
		$("#colorGreenLightness1").val(status.green_brightness);
		$("#colorBlueContrast1").val(status.blue_contrast);
		$("#colorBlueLightness1").val(status.blue_brightness);
		$("#colorSaturation1").val(status.saturation);
		$("#colorHue1").val(status.hue);

		$("#colorContrast2").val(status.contrast);
		$("#colorLightness2").val(status.brightness);
		$("#colorRedContrast2").val(status.red_contrast);
		$("#colorRedLightness2").val(status.red_brightness);
		$("#colorGreenContrast2").val(status.green_contrast);
		$("#colorGreenLightness2").val(status.green_brightness);
		$("#colorBlueContrast2").val(status.blue_contrast);
		$("#colorBlueLightness2").val(status.blue_brightness);
		$("#colorSaturation2").val(status.saturation);
		$("#colorHue2").val(status.hue);

		$("#led_frequency").val(status.led_frequency);
		$("#led_order").val(status.led_order);
		$("#led_zero").val(status.led_zero);
		$("#led_one").val(status.led_one);
	}

	function statusError(msg) {
		console.error(msg);
		$("#statusErrorMessage").html(msg);
		$('#modalError').modal('show');
	}

	function doInit() {
		disconnectTimeout = null;
		websocket = new WebSocket("ws://" + window.location.host + "/ws");
		websocket.onopen = function (evt) {
			onOpen(evt);
		};
		websocket.onclose = function (evt) {
			onClose(evt);
		};
		websocket.onmessage = function (evt) {
			onMessage(evt);
		};
		websocket.onerror = function (evt) {
			onClose(evt);
		};
	}

	function doApplyNetwork() {
		loading(true);
		var msg = {
			id: "config.network",
			udp_port: parseInt($("#statusRTPPort").val()),
			udp_server: $("#statusRegistrationServer").val(),
			wifi_sta_ssid: $("#statusClientSSID").val(),
			wifi_sta_password: $("#statusClientPassword").val(),
			wifi_ap_ssid: $("#statusAPSSID").val(),
			wifi_ap_password: $("#statusAPPassword").val(),
		};
		doSend(msg);
		reboot();
	}

	function doApplyLed() {
		loading(true);
		var msg = {
			id: "config.led",
			refresh_rate: Number($("#refresh_rate").val()),
			prefix_leds: Number($("#prefix_leds").val()),
			channels: led_lines,
			artnet_universe_offset: Number($("#artnet_universe_offset").val()),
			artnet_width: Number($("#artnet_width").val()),
			leds: [],
			led_frequency: Number($("#led_frequency").val()),
			led_order: Number($("#led_order").val()),
			led_zero: Number($("#led_zero").val()),
			led_one: Number($("#led_one").val())
		};
		for (var i = 0; $("#l" + i + "sx"); i++) {
			if ($("#l" + i + "sx").length == 0)
				break;

			msg.leds[i] = {};
			msg.leds[i].sx = Number($("#l" + i + "sx").val());
			msg.leds[i].sy = Number($("#l" + i + "sy").val());
			msg.leds[i].ox = Number($("#l" + i + "ox").val());
			msg.leds[i].oy = Number($("#l" + i + "oy").val());
			msg.leds[i].s = $("#l" + i + "s").prop('selectedIndex');
			msg.leds[i].r = $("#l" + i + "r").prop('selectedIndex');
			msg.leds[i].op = $("#l" + i + "op").val();
		}
		console.log(msg);
		doSend(msg);
	}

	function doApplyPlaylist() {
		loading(true);
		var msg = {
			id: "config.playlist",
			playlist: [],
		};
		for (var i = 0; $("#p" + i + "filename"); i++) {
			if ($("#p" + i + "filename").length == 0)
				break;

			msg.playlist[i] = {};
			msg.playlist[i].duration = Number($("#p" + i + "duration").val());
			msg.playlist[i].filename = $("#p" + i + "filename").text();
			if ($("#p" + i).is(":visible")) {
				msg.playlist[i].mode = $("#p" + i + "mode").val();
			}
			else {
				msg.playlist[i].mode = 0;
			}
		}
		console.log(msg);
		doSend(msg);
	}

	function onOpen(evt) {
		$('#modalDisconnect').modal('hide');
		loading(false);
		doSend({ id: "hello" });
	}

	function onClose(evt) {
		if (disconnectTimeout)
			return;
		console.log("disconnected");
		$('#modalDisconnect').modal('show');
		disconnectTimeout = setTimeout(doInit, 2000);
	}

	function onMessage(evt) {
		if (evt.data == "ERROR")
			return;
		var msg = JSON.parse(evt.data);
		if (msg.id == "status") {
			statusUpdate(msg);
		} else if (msg.id == "config") {
			configUpdate(msg);
		} else if (msg.id == "error") {
			statusError("The Controller reports:<br/>" + msg.cause);
			return;
		}
		loading(false);
	}

	function doSend(message) {
		message = JSON.stringify(message);
		websocket.send(message);
	}

	function reboot() {
		var counter = 0;
		setInterval(function () {
			counter++;
			if (counter > 100)
				location.reload();
			else {
				$("#rebootBar").html(counter + "%");
				$("#rebootBar")[0].style.width = counter + '%';
			}
		}, 100);
	}

	function doAdmin(command) {
		doSend({
			id: command
		});
		reboot();
	}

	function doUpload() {
		var counter = -1;

		$.ajax({
			url: '/upload',
			type: 'POST',
			data: $('#updateFile')[0].files[0],
			cache: false,
			contentType: false,
			processData: false,
			timeout: 0,
			xhr: function () {
				var myXhr = $.ajaxSettings.xhr();
				console.log("myXhr", myXhr);
				if (myXhr.upload) {
					myXhr.upload.addEventListener('progress', function (e) {
						if (counter >= 0) {
							$("#updatingBar").html(counter + "%");
							$("#updatingBar")[0].style.width = counter + '%';
						}
						if (e.lengthComputable)
							counter = Math.round(e.loaded / e.total * 50.);
						else
							counter++;
					}, false);
					myXhr.upload.addEventListener('load', function (e) {
						console.log("load", e);
						counter = 50;
						$.ajax({
							url: '/reboot',
							type: 'POST',
							data: "reboot",
							cache: false,
							contentType: false,
							processData: false,
							timeout: 0
						});

						setInterval(function () {
							if (counter == 100) {
								location.reload();
							} else {
								counter++;
								$("#updatingBar").html(counter + "%");
								$("#updatingBar")[0].style.width = counter
									+ '%';
							}
						}, 100);
					}, false);
					myXhr.upload.addEventListener('error', function (e) {
						console.error("error", e);
					}, false);
				}
				return myXhr;
			},
		});
	}

	function doPlaylistUpload() {
		var counter = -1;

		$.ajax({
			url: '/playlistUpload',
			type: 'POST',
			data: $('#updatePlaylistFile')[0].files[0],
			cache: false,
			contentType: false,
			processData: false,
			timeout: 0,
			xhr: function () {
				var myXhr = $.ajaxSettings.xhr();
				console.log("myXhr", myXhr);
				if (myXhr.upload) {
					myXhr.upload.addEventListener('progress', function (e) {
						if (counter >= 0) {
							$("#updatingBar").html(counter + "%");
							$("#updatingBar")[0].style.width = counter + '%';
						}
						if (e.lengthComputable)
							counter = Math.round(e.loaded / e.total * 50.);
						else
							counter++;
					}, false);
					myXhr.upload.addEventListener('load', function (e) {
						console.log("load", e);
						counter = 50;
						$.ajax({
							url: '/reboot',
							type: 'POST',
							data: 'reboot',
							cache: false,
							contentType: false,
							processData: false,
							timeout: 0
						});

						setInterval(function () {
							if (counter == 100) {
								location.reload();
							} else {
								counter++;
								$("#updatingBar").html(counter + "%");
								$("#updatingBar")[0].style.width = counter
									+ '%';
							}
						}, 100);
					}, false);
					myXhr.upload.addEventListener('error', function (e) {
						console.error("error", e);
					}, false);
				}
				return myXhr;
			},
		});
	}

	var msgToSend;
	var msgSendTimer;

	function msgSender() {
		if (msgToSend)
			doSend(msgToSend);
		msgToSend = null;
		msgSendTimer = null;
	}

	function doColorChange(e) {
		if (e.value < -100)
			e.value = -100;
		if (e.value > 100)
			e.value = 100;

		var name = e.id.substring(0, e.id.length - 1);
		var no = e.id.substring(e.id.length - 1);
		var eo;
		if (no == "1")
			eo = $("#" + name + "2")[0];
		else
			eo = $("#" + name + "1")[0];
		eo.value = e.value;

		var msg = {
			id: "config.color",
			contrast: Number($("#colorContrast1").val()),
			brightness: Number($("#colorLightness1").val()),
			red_contrast: Number($("#colorRedContrast1").val()),
			red_brightness: Number($("#colorRedLightness1").val()),
			green_contrast: Number($("#colorGreenContrast1").val()),
			green_brightness: Number($("#colorGreenLightness1").val()),
			blue_contrast: Number($("#colorBlueContrast1").val()),
			blue_brightness: Number($("#colorBlueLightness1").val()),
			saturation: Number($("#colorSaturation1").val()),
			hue: Number($("#colorHue1").val())
		};
		if (msgSendTimer) {
			msgToSend = msg;
		} else {
			doSend(msg);
			msgSendTimer = setTimeout(msgSender, 500);
		}
	}

	function doColorDiscard(e) {
		msgToSend = null;
		doSend({
			id: "config.color.discard"
		});
	}

	function doColorDefaults(e) {
		msgToSend = null;
		doSend({
			id: "config.color.defaults"
		});
	}

	function doColorWrite(e) {
		if (msgToSend) {
			doSend(msgToSend);
			msgToSend = null;
		}
		doSend({
			id: "config.color.write"
		});
	}

	function doDeletePlaylistEntry(e) {
		var id = Number(e.parentElement.parentElement.parentElement.id.substring(1));

		for (var i = id; i < playlist_lines - 1; i++) {
			var dst = $("#p" + i + "duration");
			var src = $("#p" + (i + 1) + "duration");
			dst.val(src.val());
			dst = $("#p" + i + "filename");
			src = $("#p" + (i + 1) + "filename");
			dst.text(src.text());
			dst = $("#p" + i + "mode");
			src = $("#p" + (i + 1) + "mode");
			dst.val(src.val());
		}
		$("#p" + i).hide();
		$("#p" + i + "duration").val(0);
		$("#p" + i + "filename").val("");
	}

	function doUpPlaylistEntry(i) {
		if (i >= 1 && i < playlist_lines) {

			var src = $("#p" + i + "duration");
			var dst = $("#p" + (i - 1) + "duration");
			var c = dst.val();
			dst.val(src.val())
			src.val(c);
			dst = $("#p" + i + "filename");
			src = $("#p" + (i - 1) + "filename");
			c = dst.text();
			dst.text(src.text());
			src.text(c);
			dst = $("#p" + i + "mode");
			src = $("#p" + (i - 1) + "mode");
			c = dst.prop('selectedIndex');
			dst.prop('selectedIndex', src.prop('selectedIndex'));
			src.prop('selectedIndex', c);
		}
	}

	function doAddPlaylistFile() {
		$('#modalPlaylistFile').modal('show');
	}

	function doAddPlaylistUrl() {
		$('#modalPlaylistURL').modal('show');
	}

	return {
		init: function () {
			doInit();
		},
		applyNetwork: function () {
			doApplyNetwork();
		},
		applyLed: function () {
			doApplyLed();
		},
		applyPlaylist: function () {
			doApplyPlaylist();
		},
		reboot: function () {
			doAdmin("reboot");
		},
		defaults: function () {
			doAdmin("defaults");
		},
		upload: function () {
			return doUpload();
		},
		colorChange: function (e) {
			return doColorChange(e);
		},
		colorDiscard: function () {
			return doColorDiscard();
		},
		colorDefaults: function () {
			return doColorDefaults();
		},
		colorWrite: function () {
			return doColorWrite();
		},
		deletePlaylistEntry: function (element) {
			return doDeletePlaylistEntry(element);
		},
		upPlaylistEntry: function (e) {
			return doUpPlaylistEntry(Number(e.parentElement.parentElement.parentElement.id.substring(1)));
		},
		downPlaylistEntry: function (e) {
			return doUpPlaylistEntry(Number(e.parentElement.parentElement.parentElement.id.substring(1)) + 1);
		},
		addPlaylistFile: function () {
			return doAddPlaylistFile();
		},
		addPlaylistUrl: function () {
			return doAddPlaylistUrl();
		},
		playlistUpload: function () {
			return doPlaylistUpload();
		}
	}

})();