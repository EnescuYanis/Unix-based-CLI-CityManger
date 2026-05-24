CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -D_GNU_SOURCE

.PHONY: all clean

all: city_manager monitor_reports scorer city_hub

city_manager: main.c commands.c commands.h
	$(CC) $(CFLAGS) -o city_manager main.c commands.c

monitor_reports: monitor_reports.c
	$(CC) $(CFLAGS) -o monitor_reports monitor_reports.c

scorer: scorer.c commands.h
	$(CC) $(CFLAGS) -o scorer scorer.c

city_hub: cityhub.c
	$(CC) $(CFLAGS) -o city_hub cityhub.c

clean:
	rm -f city_manager monitor_reports scorer city_hub
