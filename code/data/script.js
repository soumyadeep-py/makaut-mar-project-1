const connectionDot =
    document.getElementById("connectionDot");

const connectionText =
    document.getElementById("connectionText");

const systemStatus =
    document.getElementById("systemStatus");

const wifiStatus =
    document.getElementById("wifiStatus");

const systemMessage =
    document.getElementById("systemMessage");


// Sensor elements

const soilValue =
    document.getElementById("soilValue");

const soilState =
    document.getElementById("soilState");


const temperatureValue =
    document.getElementById("temperatureValue");

const temperatureState =
    document.getElementById("temperatureState");


const humidityValue =
    document.getElementById("humidityValue");

const humidityState =
    document.getElementById("humidityState");


const lightValue =
    document.getElementById("lightValue");

const lightState =
    document.getElementById("lightState");


// ======================================================
// CONNECTION STATUS
// ======================================================

function setConnectionStatus(connected) {

    if (connected) {

        connectionDot.classList.remove(
            "offline"
        );

        connectionDot.classList.add(
            "online"
        );

        connectionText.textContent =
            "ESP32 Connected";

    } else {

        connectionDot.classList.remove(
            "online"
        );

        connectionDot.classList.add(
            "offline"
        );

        connectionText.textContent =
            "Connection Lost";
    }
}


// ======================================================
// SENSOR DISPLAY
// ======================================================

function updateSensor(
    valueElement,
    stateElement,
    sensor
) {

    const state =
        sensor.state;


    stateElement.textContent =
        state;


    // Sensor missing / invalid

    if (
        sensor.value === null ||
        sensor.value === undefined
    ) {

        valueElement.textContent =
            "—";

        return;
    }


    // Display reading

    valueElement.textContent =
        Number(sensor.value).toFixed(1);
}


// ======================================================
// SYSTEM MESSAGE
// ======================================================

function updateSystemMessage(data) {

    if (data.system === "ALERT") {

        systemMessage.textContent =
            "One or more plant conditions have exceeded the configured threshold.";

        systemStatus.textContent =
            "ALERT";

        systemStatus.classList.remove(
            "normal"
        );

        systemStatus.classList.add(
            "alert"
        );

    } else {

        systemMessage.textContent =
            "Plant conditions are currently within the configured safe range.";

        systemStatus.textContent =
            "NORMAL";

        systemStatus.classList.remove(
            "alert"
        );

        systemStatus.classList.add(
            "normal"
        );
    }
}


// ======================================================
// WIFI STATUS
// ======================================================

function updateWiFiStatus(data) {

    if (data.wifi) {

        wifiStatus.textContent =
            "ON";

    } else {

        wifiStatus.textContent =
            "OFF";
    }
}


// ======================================================
// GET SENSOR DATA
// ======================================================

async function updateData() {

    try {

        const response =
            await fetch(
                "/api",
                {
                    cache: "no-store"
                }
            );


        if (!response.ok) {

            throw new Error(
                "API request failed"
            );
        }


        const data =
            await response.json();


        setConnectionStatus(true);


        // System

        updateSystemMessage(data);

        updateWiFiStatus(data);


        // Sensors

        updateSensor(
            soilValue,
            soilState,
            data.soil
        );


        updateSensor(
            temperatureValue,
            temperatureState,
            data.temperature
        );


        updateSensor(
            humidityValue,
            humidityState,
            data.humidity
        );


        updateSensor(
            lightValue,
            lightState,
            data.light
        );


    } catch (error) {

        console.log(
            "ESP32 connection error:",
            error
        );


        setConnectionStatus(false);


        systemMessage.textContent =
            "Unable to communicate with the ESP32.";

    }
}


// ======================================================
// AUTO REFRESH
// ======================================================

updateData();

setInterval(
    updateData,
    1000
);
