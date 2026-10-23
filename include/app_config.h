#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define FW_VERSION  "0.1.0"
#define FW_BATCH    "LOT-0001"

/* mettre a 1 pour tester les boutons au demarrage */
#define APP_DEBUG_BUTTONS      0

/* 60000 = minutes, mettre 1000 pour tester plus vite */
#define LOG_INTERVAL_UNIT_MS   60000UL

#define LONG_PRESS_MS          5000UL
#define CONFIG_INACTIVITY_MS   (30UL * 60UL * 1000UL) /* mettre moins pour tester */
#define MAINTENANCE_PERIOD_MS  5000UL


#define ECO_EXIT_WITH_GREEN    1

#endif
