#ifndef __DWEET_H
#define __DWEET_H

#include <ESP8266HTTPClient.h>

boolean SendDweet(char *buf);

#ifndef DWEET_URL
#define DWEET_URL "http://dweet.io/dweet/quietly/for/c440d18e-4fa7-4eb7-bafb-499b19de97ab"
#endif

#endif // __DWEET_H
