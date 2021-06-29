// Use this file to store all of the private credentials and connection details

// WiFi configuration
#define SECRET_SSID "SSID" // SSID
#define SECRET_PASS "PASSWORD" // WiFi password

// MQTT access
#define MQTT_BROKERIP "IP"           // IP address of the machine running the MQTT broker
#define MQTT_CLIENTID "clientid"                 // client identifier
#define MQTT_USERNAME "username"            // mqtt user's name
#define MQTT_PASSWORD "password"            // mqtt user's password

// InfluxDB cfg
#define INFLUXDB_URL "url"   // IP and port of the InfluxDB server
#define INFLUXDB_TOKEN "token"   // API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_ORG "id"                 // organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_BUCKET "bucket"                 // bucket name (Use: InfluxDB UI -> Load Data -> Buckets)


// OpenWeatherApi key
#define KEY "key"

// MySQL
#define MYSQL_IP {127, 0, 0, 1} // ip mysql
#define MYSQL_USER "username"
#define MYSQL_PASS "password"
