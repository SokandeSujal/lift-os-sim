CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -pthread -Iinclude
LDLIBS := -pthread -lrt
SRC := src/main.c src/core.c src/scenario.c src/passenger.c src/board.c src/render_term.c src/export.c
OBJ := $(SRC:.c=.o)
.PHONY: all test smoke integration asan clean
all: lift_sim
lift_sim: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
tests/test_core: tests/test_core.c src/core.c src/scenario.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
test: tests/test_core
	./tests/test_core
smoke: lift_sim
	@./lift_sim --scenario scenarios/tiny_a.csv --policy fcfs --no-ui --fast > out/smoke_fcfs.txt
	@./lift_sim --scenario scenarios/tiny_a.csv --policy nearest --no-ui --fast > out/smoke_nearest.txt
	@grep -q 'Average wait: 14.00' out/smoke_fcfs.txt
	@grep -q 'Makespan: 35' out/smoke_fcfs.txt
	@grep -q 'Average wait: 7.00' out/smoke_nearest.txt
	@grep -q 'Makespan: 14' out/smoke_nearest.txt
	@echo 'PASS live golden smoke'
integration: lift_sim
	@./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast > out/sync.txt
	@./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast --no-sync > out/nosync.txt
	@grep -q 'Board corruption: 0' out/sync.txt
	@awk -F': ' '/Board corruption:/{v=$$2}END{exit !(v+0>0)}' out/nosync.txt
	@./lift_sim --scenario scenarios/rush_morning.csv --policy fcfs --no-ui --tick-ms 1 > out/fcfs_check.txt
	@./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --tick-ms 1 > out/nearest_check.txt
	@awk 'FNR==NR&&/Average wait:/{f=$$3}FNR!=NR&&/Average wait:/{n=$$3}END{exit !(n<f)}' out/fcfs_check.txt out/nearest_check.txt
	@awk 'FNR==NR&&/Average load:/{f=$$3+0}FNR!=NR&&/Average load:/{n=$$3+0}END{exit !(n>f)}' out/fcfs_check.txt out/nearest_check.txt
	@echo 'PASS synchronization and policy integration'
asan: CFLAGS := -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -pthread -Iinclude
asan: clean all
clean:
	rm -f $(OBJ) lift_sim tests/test_core out/*.js out/*.csv out/*.txt
