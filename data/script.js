
// ===================================
// GET ESP8266 CONNECTION STATUS
// ===================================

async function updateStatus() {

    try {

        const response = await fetch("/api/status");
        const data = await response.json();

        const status = document.getElementById("connectionStatus");
        const ssid = document.getElementById("wifiSSID");
        const deviceIP = document.getElementById("deviceIP");
        const apIP = document.getElementById("apIP");

        if (status) {
            status.textContent = data.connected
                ? "Connected"
                : "Disconnected";

            status.style.color = data.connected
                ? "green"
                : "red";
        }

        if (ssid) {
            ssid.textContent = data.ssid || "Not connected";
        }

        if (deviceIP) {
            deviceIP.textContent = data.staIP;
        }

        if (apIP) {
            apIP.textContent = data.apIP;
        }

    } catch (error) {

        console.error("Status update failed:", error);

    }
}


// ===================================
// WIFI CONFIGURATION FORM
// ===================================

const wifiForm = document.getElementById("wifiForm");

if (wifiForm) {

    wifiForm.addEventListener("submit", async function(event) {

        event.preventDefault();

        const message = document.getElementById("message");

        const ssid = document.getElementById("ssid").value;
        const password = document.getElementById("password").value;

        message.textContent = "Connecting to Wi-Fi...";
        message.style.color = "#007bff";

        try {

            const response = await fetch("/connect", {

                method: "POST",

                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },

                body: new URLSearchParams({
                    ssid: ssid,
                    password: password
                })

            });

            const result = await response.json();

            message.textContent = result.message;

            message.style.color = result.success
                ? "green"
                : "red";

            if (result.success) {
                setTimeout(() => {
                    window.location.href = "/";
                }, 1500);
            }

        } catch (error) {

            message.textContent = "Unable to reach ESP8266.";
            message.style.color = "red";

        }

    });

}


// ===================================
// AUTOMATIC STATUS REFRESH
// ===================================

updateStatus();

setInterval(updateStatus, 3000);
