#ifdef __MQTT_H

void MQTT_ConnectAndSend(String clientId, char * topic, char *buf) 
{  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  
  // Loop until we're reconnected
  int retries = 3;
  while (!mqttClient.connected() && retries > 0) 
  {  
    DEBUG_PRINT("Attempting new MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) 
    {
      DEBUG_PRINTLN("connected");
      // Once connected, publish an announcement...
      DEBUG_PRINTLN("Sending:");
      DEBUG_PRINTLN(buf);
      mqttClient.publish(topic, buf);
    }
    else
    {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(mqttClient.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 1 seconds before retrying
      delay(1000);
    }

    retries -= 1;
  }
}

#endif
