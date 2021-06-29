#!/usr/bin/env python
# coding: utf-8

# In[ ]:


from telegram.ext import Updater, CommandHandler, MessageHandler, Filters
import paho.mqtt.client as mqtt
import json
import requests as req
import pandas as pd
from telegram import ParseMode
import datetime
from geopy.geocoders import Nominatim
import mysql.connector as mysql

TOKEN="token telegram"# telegram
api_key = ""# api openweather map

db = mysql.connect(
    host = "ip",
    user = "username",
    passwd = "password",
    database = "db"
)
cursor = db.cursor()
upd= Updater(TOKEN, use_context=True)


# In[ ]:


def insert_new_id(id):
    query = "SELECT * FROM users"
    global cursor
    ## getting records from the table
    cursor.execute(query)

    ## fetching all records from the 'cursor' object
    records = cursor.fetchall()
    check = False
    ## Showing the data
    for record in records:
        if record[0] == id:
            check = True
            break
    if (not check):
        cursor.execute("INSERT INTO users(id) VALUES ("+id+")")
        db.commit()
        print(cursor.rowcount, "record inserted")
        return True
    return False


# In[ ]:


def on_connect(client, userdata, flags, rc):
    client.subscribe('gferrario/home')
    client.subscribe('gferrario/weather')
    client.subscribe('gferrario/ack')


# In[ ]:


def get_records():
    query = "SELECT * FROM users"
    cursor.execute(query)
    records = cursor.fetchall()
    return records


# In[ ]:


def on_message(client, userdata, message):
    data = dict(
        topic=message.topic,
        payload=message.payload.decode()
    )
    global upd
    global cursor
    if data["topic"] == "gferrario/home": 
        json_home = json.loads(data["payload"])
        print(json_home)
        temp=str(json_home["temperature"])
        if(json_home["check_temp"]>0):
            temp="*"+ temp +"* °C 🔴"
        rssi=str(json_home["rssi"])
        if(json_home["check_wifi"]):
            rssi="*"+rssi+"* 🔴"
        light=str(json_home["light"])
        if(json_home["check_light"]):
            light="*"+light+"* 🔴"
        humidity=str(json_home["humidity"])+"%"
        tilt=str(json_home["tilt"])
        if(json_home["check_tilt"]):
            tilt="*"+tilt+"* 🔴"
        records=get_records()
        for record in records:
            upd.bot.send_message(chat_id=record[0], text="Home Monitoring:\nTemperature: "+ temp +"\nLight: "+ light +"\nHumidity: "+ humidity +"\nWiFi: "+rssi+"\nTilt: "+tilt,
                parse_mode=ParseMode.MARKDOWN)
    
    elif data["topic"] == "gferrario/weather":
        json_weather = json.loads(data["payload"])

        weather_condition = json_weather["weather_condition"]
        temperature = round(json_weather["temperature"], 2)
        humidity = json_weather["humidity"]
        pressure = json_weather["pressure"]
        wind = json_weather["wind"]
        weather="☀️"
        if("cloud" in weather_condition):
            weather="☁️"
        elif("rain" in weather_condition):
            weather="🌧️"
        
        records=get_records()
        for record in records:
            upd.bot.send_message(chat_id=record[0], text="Weather Monitoring:\nTemperature: "+ str(temperature) +" °C\nHumidity: "+ str(humidity) +"%\nPressure: "+str(pressure)+"\nWind Speed: "+str(wind)+" m/s\nWeather: "+weather,
                parse_mode=ParseMode.MARKDOWN)
    elif data["topic"] == "gferrario/ack":
        doc = json.loads(data["payload"])
        print("ack")
        if(doc["status"] == "done"):
            records=get_records()
            for record in records:
                upd.bot.send_message(chat_id=record[0], text="Success: "+doc["goal"])


# In[ ]:


client = mqtt.Client("telegram")
client.username_pw_set("gferrario","iot817518")
client.on_connect = on_connect
client.on_message = on_message

client.connect("149.132.178.180",1883, 60)


# In[ ]:


def setup(update, context):
    global client
    sleepTime=float(update.message.text.split()[1].strip())
    executionTime=float(update.message.text.split()[2].strip())
    if(sleepTime >=0 and executionTime>=0):
        client.publish("gferrario/setup",  '{"sleep_time": "'+ str(sleepTime) +'", "execution_time": "'+ str(executionTime) +'"}', retain=True, qos=1)
        capacity=5.2
        duration_hour = capacity / (((sleepTime * 10**(-6)) * (24 * 10**(-6)) + (executionTime * 10**(-6) ) * (170 * 10**(-3))) /(sleepTime * 10**(-6) + executionTime * 10**(-6)))
        days=duration_hour/24
        hours=int(float(str(days -int(days))[1:])*24)
        context.bot.send_message(chat_id=update.effective_chat.id, text="Estimated 🔋 time: "+str(int(days))+" days and "+str(hours)+" hours")


# In[ ]:


def home(update, context):
    global client
    alarms = {
                "slave" : "NodeMCUHome",
                "status": "activated"
            }
    client.publish("gferrario/slave/Home",  json.dumps(alarms), retain=True, qos=1)
    #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: home monitoring activated")


# In[ ]:


def weather(update, context):
    global client
 
    client.publish("gferrario/slave/Weather",  '{"slave" : "NodeMCUWeather","city": "Muggiò", "status": "activated"}', retain=True, qos=1)
    #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: weather station activated")


# In[ ]:


def forecast(update, context):
    global client
    address=str(update.message.text.split()[1].strip())
 
    if address !="":
        geolocator = Nominatim(user_agent="Geolocalization")
        location = geolocator.geocode(address)
        #print(location.address)
        url = "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&exclude=daily,current,minutely,alerts&appid=%s" % (location.latitude, location.longitude, api_key)
        response = req.get(url)
        data = json.loads(response.text)
        test = pd.DataFrame(data["hourly"])
        test= test[["dt", "weather"]]
        test["dt"] = test.apply(lambda x: datetime.datetime.fromtimestamp(x["dt"]).strftime('%Y-%m-%d %H:%M'), axis=1)
        test["weather"] = test.apply(lambda x: x["weather"][0]["description"], axis=1)
        test["check"] = test.apply(lambda x: datetime.datetime.strptime(x["dt"], '%Y-%m-%d %H:%M').hour%6 == 0, axis=1)
        test = test[test["check"]]
        del test['check']
        weather_forecasting="City: "+address+"\n"
        for _, row in test.iterrows():
            weather_forecasting += str(row["dt"])+"    "
            if('rain' in row["weather"]):
                weather_forecasting += "🌧️\n"
            elif 'sky' in row["weather"]:
                weather_forecasting += "☀️\n"
            elif 'cloud' in row["weather"]:
                weather_forecasting += "☁️\n"
            elif 'snow' in row["weather"]:
                weather_forecasting += "🌨️\n"
            elif 'thunderstorm' in row["weather"]:
                weather_forecasting += "🌩️\n"
            elif 'mist' in row["weather"]:
                weather_forecasting += "🌫️\n"
        context.bot.send_message(chat_id=update.effective_chat.id, text=weather_forecasting)
    else:
        context.bot.send_message(chat_id=update.effective_chat.id, text="Error: wrong city")


# In[ ]:


def alert(update, context):
    global client
    sensor=str(update.message.text.split()[1].strip()).lower()
    if(sensor == "light" or sensor == "temperature"):
        min=float(update.message.text.split()[2].strip())
        max=float(update.message.text.split()[3].strip())
        if((min >= -10 and max <= 40 and sensor=="temperature") or (min >= 0 and max <= 1024 and sensor=="light") and min<max):
            client.publish("gferrario/homeAlert/"+sensor,  '{"sensor" : "'+ sensor +'", "min": "'+ str(min) +'", "max": "'+ str(max) +'"}', retain=True, qos=1)
            #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: alerts set correctly for "+ sensor +" sensor")
        else:
            context.bot.send_message(chat_id=update.effective_chat.id, text="Error: wrong alerts for "+ sensor +" sensor")
    elif(sensor == "wifi"):
        min=float(update.message.text.split()[2].strip())
        if(min >= -80 and min <= 0):
            client.publish("gferrario/homeAlert/"+sensor,  '{"sensor" : "'+ sensor +'", "min": "'+ str(min) +'"}', retain=True, qos=1)
            #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: alerts set correctly for "+ sensor +" sensor")
        else:
            context.bot.send_message(chat_id=update.effective_chat.id, text="Error: wrong alerts for "+ sensor +" sensor")
    
    else:
        context.bot.send_message(chat_id=update.effective_chat.id, text="Error: unrecognised sensor")


# In[ ]:


def stopHome(update, context):
    global client
    client.publish("gferrario/slave/Home",  "{'slave': 'NodeMCUHome', 'status': 'deactivated'}", retain=True, qos=1)
    #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: home monitoring deactivated")


# In[ ]:


def stopWeather(update, context):
    global client
    print(update.effective_chat.id)
    client.publish("gferrario/slave/Weather",  "{'slave': 'NodeMCUWeather', 'status': 'deactivated'}", retain=True, qos=1)
    #context.bot.send_message(chat_id=update.effective_chat.id, text="Success: weather station deactivated")


# In[ ]:


def registerMe(update, context):
    chat_id=str(update.effective_chat.id)
    flag=insert_new_id(chat_id)
    if flag:
        context.bot.send_message(chat_id, text="Success: registration completed")
    else: 
        context.bot.send_message(chat_id, text="Warning: already registered")


# In[ ]:


def removeMe(update, context):
    global cursor
    chat_id=str(update.effective_chat.id)
    query = "DELETE FROM users WHERE id ="+chat_id
    cursor.execute(query)
    
    query = "SELECT * FROM users"
    cursor.execute(query)
    records = cursor.fetchall()
    
    check = False
    db.commit()
    for record in records:
        if record[0] == id:
            check = True
            break
    if (not check):
        context.bot.send_message(chat_id, text="Success: deletion completed")
    else:
        context.bot.send_message(chat_id, text="Warning: you are not registered")


# In[ ]:


def help(update, context):
    print(update.effective_chat.id)
    general="*General*:\n/setup sleepTime executionTime - updates the sleep time and the execution time of the ESP8266\n/registerMe - registers the user with the monitoring system\n/removeMe - removes user registration from the monitoring system\n\n"
    home="*Home Monitoring:*\n/home - activates home monitoring\n/stopHome - deactivates home monitoring\n/setAlert sensor lowerBound upperBound - sets the thresholds for the indicated sensor\n\n"
    weather="*Weather Station*:\n/weather - activates weather monitoring\n/stopWeather - deactivates the weather monitoring\n/forecasting city - weather forecasting for the indicated city\n\n"
    plus="Possible value for setAlert:\n   1) light lowerBound upperBound\n   2) temperature lowerBound upperBound\n   3) wifi lowerBound"
    context.bot.send_message(chat_id=update.effective_chat.id, 
                             text=general+home+weather+plus, 
                             parse_mode=ParseMode.MARKDOWN)


# In[ ]:


def main():

    global upd
    disp=upd.dispatcher
    disp.add_handler(CommandHandler("help", help))
    disp.add_handler(CommandHandler("setup", setup))
    disp.add_handler(CommandHandler("home", home))
    disp.add_handler(CommandHandler("weather", weather))
    disp.add_handler(CommandHandler("stopHome", stopHome))
    disp.add_handler(CommandHandler("stopWeather", stopWeather))
    disp.add_handler(CommandHandler("setAlert", alert))
    disp.add_handler(CommandHandler("forecasting", forecast))
    disp.add_handler(CommandHandler("registerMe", registerMe))
    disp.add_handler(CommandHandler("removeMe", removeMe))
    upd.start_polling()

    client.loop_forever()


# In[ ]:


if __name__=='__main__':
    main()


# In[ ]:




