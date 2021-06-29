from flask import Flask, render_template, url_for, request, redirect
from flask_mqtt import Mqtt
import json

app = Flask(__name__, static_url_path='')
app.config['MQTT_BROKER_URL'] = '149.132.178.180'
app.config['MQTT_BROKER_PORT'] = 1883
app.config['MQTT_USERNAME'] = 'gferrario'
app.config['MQTT_PASSWORD'] = 'iot817518'
app.config['MQTT_REFRESH_TIME'] = 1.0  # refresh time in seconds
mqtt = Mqtt(app)

# global variables
json_home = {}
json_weather = {}


@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    mqtt.subscribe('gferrario/home')
    mqtt.subscribe('gferrario/weather')

@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):
    data = dict(
        topic=message.topic,
        payload=message.payload.decode()
    )
    if data["topic"] == "gferrario/home":
        print("HOME:")
        global json_home 
        json_home = json.loads(data["payload"])
        print(json_home)
    elif data["topic"] == "gferrario/weather":
        print("WEATHER:")
        global json_weather
        json_weather = json.loads(data["payload"])
        print(json_weather)

@app.route("/home")
def home():
    if(not bool(json_home)):
        return render_template('home.html', temperature = "", humidity = "", light = "", wifi = "", tilt = "", check_temp = 0, check_tilt = 0, check_light = 0, check_wifi = 0)
    else:

        return render_template('home.html', temperature = float(json_home["temperature"]), humidity = json_home["humidity"], light = json_home["light"], wifi = json_home["rssi"], tilt = json_home["tilt"], check_temp = json_home["check_temp"], check_tilt = json_home["check_tilt"], check_light = json_home["check_light"], check_wifi = json_home["check_wifi"])

@app.route("/weather")
def weather():
    if(not bool(json_weather)):
        return render_template('weather.html', weather_condition = "", temperature = "", humidity = "", pressure = "", wind = "")
    else:
        return render_template('weather.html', weather_condition = json_weather["weather_condition"], temperature = json_weather["temperature"], humidity = json_weather["humidity"], pressure = json_weather["pressure"], wind = json_weather["wind"])

@app.route("/")
def index():
	return render_template('index.html')
    
@app.route('/alerts', methods =["GET", "POST"])
def alerts():
    if request.method == "POST":
       
       minTemp = request.form.get("minTemp")
       maxTemp = request.form.get("maxTemp")
       minLight = request.form.get("minL")
       maxLight = request.form.get("maxL")
       minWifi = request.form.get("minW")
       
       alarms = {   
                    "slave" : "NodeMCUHome",
                    "lower_bound_temperature" : minTemp,
                    "upper_bound_temperature" : maxTemp,
                    "lower_bound_light" : minLight,
                    "upper_bound_light" : maxLight,
                    "lower_bound_rssi" : minWifi
                }
       mqtt.publish('gferrario/activeSlaveHome', json.dumps(alarms))
       return redirect(url_for('home'))
    return render_template("form_home.html")

@app.route('/api', methods =["GET", "POST"])
def api():
    if request.method == "POST":
       
       city = request.form.get("city")
       countryCode = request.form.get("countryCode")
       
       location = {   
                    "slave" : "NodeMCUWeather",
                    "city" : city,
                    "countryCode" : countryCode
                }
       mqtt.publish('gferrario/activeSlaveWeather', json.dumps(location))
       return redirect(url_for('weather'))
    return render_template("form_weather.html")

@app.route("/resetHome")
def resetHome():
    global json_home 
    json_home = {}

    mqtt.publish('gferrario/deactiveSlaveHome', json.dumps({'slave': 'NodeMCUHome'}))
    return redirect(url_for('index'))

@app.route("/resetWeather")
def resetWeather():
    global json_weather
    json_weather = {}
    
    mqtt.publish('gferrario/deactiveSlaveWeather', json.dumps({'slave': 'NodeMCUWeather'}))
    return redirect(url_for('index'))
    
if __name__ == "__main__":
    app.run()