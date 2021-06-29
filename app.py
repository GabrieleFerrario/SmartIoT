import requests as req
from flask import Flask, render_template, url_for, request, redirect
from flask_mqtt import Mqtt
import json
import pandas as pd
import datetime
import json
from geopy.geocoders import Nominatim

#app = Flask(__name__)

app = Flask(__name__, static_url_path='')
app.config['MQTT_BROKER_URL'] = 'MQTT_BROKER_URL'
app.config['MQTT_BROKER_PORT'] = 'port'
app.config['MQTT_USERNAME'] = 'username'
app.config['MQTT_PASSWORD'] = 'password'
app.config['MQTT_REFRESH_TIME'] = 1.0  # refresh time in seconds
mqtt = Mqtt(app)

# global variables
json_home = {}
json_weather = {}
alarms = {}
sleep = {}
lat = ""
long = ""
sleepTime=0
executionTime=0

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
        #print("HOME:")
        global json_home 
        json_home = json.loads(data["payload"])
        #print(json_home)
    elif data["topic"] == "gferrario/weather":
        #print("WEATHER:")
        global json_weather

        json_weather = json.loads(data["payload"])
        #print(json_weather)

@app.route("/home")
def home():
    if(not bool(json_home)):
        global alarms
        if(not bool(alarms)):
            
            alarms = {
                "slave" : "NodeMCUHome",
                "status": "activated"
            }
            mqtt.publish('gferrario/slave/Home', json.dumps(alarms), retain=True, qos=1)
            
        return render_template('home.html', temperature = "", humidity = "", light = "", wifi = "", tilt = "", check_temp = 0, check_tilt = 0, check_light = 0, check_wifi = 0)
    else:

        return render_template('home.html', temperature = float(json_home["temperature"]), humidity = json_home["humidity"], light = json_home["light"], wifi = json_home["rssi"], tilt = json_home["tilt"], check_temp = json_home["check_temp"], check_tilt = json_home["check_tilt"], check_light = json_home["check_light"], check_wifi = json_home["check_wifi"])

@app.route("/weather")
def weather():
    if(not bool(json_weather)):
        mqtt.publish("gferrario/slave/Weather",  '{"slave" : "NodeMCUWeather","city": "Muggiò", "status": "activated"}', retain=True, qos=1)
        return render_template('weather.html', weather_condition = "", temperature = "", humidity = "", pressure = "", wind = "")
    else:
        return render_template('weather.html', weather_condition = json_weather["weather_condition"], temperature = json_weather["temperature"], humidity = json_weather["humidity"], pressure = json_weather["pressure"], wind = json_weather["wind"])

@app.route("/forecasting")
def forecasting():
    global lat
    global long
    if(lat != "" and long != ""):
        api_key = "api openweather"
        url = "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&exclude=daily,current,minutely,alerts&appid=%s" % (lat, long, api_key)
        response = req.get(url)
        data = json.loads(response.text)
        test = pd.DataFrame(data["hourly"])
        test= test[["dt", "temp", "humidity", "pressure", "wind_speed", "weather"]]
        test["dt"] = test.apply(lambda x: datetime.datetime.fromtimestamp(x["dt"]).strftime('%Y-%m-%d %H:%M:%S'), axis=1)
        test["temp"] = test.apply(lambda x: x["temp"] - 273.15, axis=1)
        test["weather"] = test.apply(lambda x: x["weather"][0]["description"], axis=1)
        test["temp"] = test["temp"].round(2)
        test.rename(columns={'dt': 'Date', 'temp': 'Temperature (°C)', 'wind_speed' : 'Wind Speed (m/s)', 'humidity': 'Humidity (%)', 'pressure' : 'Pressure', 'weather' : 'Weather'}, inplace=True)
        test["check"] = test.apply(lambda x: datetime.datetime.strptime(x["Date"], '%Y-%m-%d %H:%M:%S').hour%6 == 0, axis=1)
        test = test[test["check"]]
        del test['check']
        headings = test.columns
        #headings = [i.capitalize() for i in headings]
        records = test.to_records(index=False)
        result = list(records)
        return render_template('forecasting.html', headings=headings, data=result)
    return render_template('form_weather.html', headings="", data="")



@app.route("/")
def index():
    global sleepTime
    global executionTime
    sleepTime = float(sleepTime)
    executionTime = float(executionTime)
    days = 0
    hours = 0
    if (sleepTime>0 and executionTime>0):
        capacity=5.2
        duration_hour = capacity / (((sleepTime * 10**(-6)) * (24 * 10**(-6)) + (executionTime * 10**(-6) ) * (170 * 10**(-3))) /(sleepTime * 10**(-6) + executionTime * 10**(-6)))
        days=duration_hour/24
        hours=int(float(str(days -int(days))[1:])*24)
    return render_template('index.html', days=int(days), hours=hours)

@app.route('/alerts', methods =["GET", "POST"])
def alerts():
    global alarms
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
       mqtt.publish('gferrario/alarms', json.dumps(alarms), retain=True, qos=1)
       return redirect(url_for('home'))
    return render_template("form_home.html")

@app.route('/api', methods =["GET", "POST"])
def api():
    if request.method == "POST":
        
        city = request.form.get("city")
        if city!="":
            geolocator = Nominatim(user_agent="Geolocalization")
            location = geolocator.geocode(city)
            global lat
            lat=location.latitude
            global long
            long=location.longitude

            return redirect(url_for('forecasting'))

    return render_template("form_weather.html")

@app.route('/sleeptime', methods =["GET", "POST"])
def sleeptime():
    global sleep
    if request.method == "POST":

        time = request.form.get("time")
        extime = request.form.get("extime")
        sleep = {   
                    "sleep_time" : time,
                    "execution_time" : extime
                }
        global sleepTime
        sleepTime = time
        global executionTime
        executionTime = extime
        mqtt.publish('gferrario/setup', json.dumps(sleep), retain=True, qos=1)

        return redirect(url_for('index'))
    if(not bool(sleep)):
        return render_template("sleep_time.html", time="", extime="")
    else:
        return render_template("sleep_time.html", time=sleep["sleep_time"], extime=sleep["execution_time"])

@app.route("/resetHome")
def resetHome():
    global json_home 
    json_home = {}

    mqtt.publish('gferrario/slave/Home', json.dumps({'slave': 'NodeMCUHome', 'status': 'deactivated'}), retain=True, qos=1)
    return redirect(url_for('index'))

@app.route("/resetWeather")
def resetWeather():
    global json_weather
    json_weather = {}
    
    mqtt.publish('gferrario/slave/Weather', json.dumps({'slave': 'NodeMCUWeather', 'status': 'deactivated'}), retain=True, qos=1)
    return redirect(url_for('index'))
    
if __name__ == "__main__":
    app.run()