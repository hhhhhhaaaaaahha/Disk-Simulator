CC=	gcc
CXX=g++
OBJS= src/lba.c src/pba.c src/batch.c src/chs.c src/record_op.c src/rw.c src/band.c src/FluidSMR.c src/cache.c src/Hybrid.c

CPPFLAGS=-std=c++11 -Wfatal-errors -Wall
CFLAGS=-Wfatal-errors -Wall -g

LDFLAGS= -Iinclude

.PHONY: clean dirs valgrind

all: dirs cmr native

cmr: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DCMR main.c -o bin/cmr $(OBJS)

native_a: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_A main.c -o bin/native_a $(OBJS)

native_b: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_B main.c -o bin/native_b $(OBJS)

fluid_smr: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DFLUIDSMR main.c -o bin/fluid_smr $(OBJS)

hybrid: clean $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DHYBRID main.c -o bin/hybrid $(OBJS)

#cmr_cache: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DCMR -DCACHE main.c -o bin/cmr_cache $(OBJS)

#native_a_cache: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_A -DCACHE main.c -o bin/native_a_cache $(OBJS)

#native_b_cache: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_B -DCACHE main.c -o bin/native_b_cache $(OBJS)

#j_cmr: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DCMR -DJOURNALING main.c -o bin/j_cmr $(OBJS)

#j_native_a: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_A -DJOURNALING main.c -o bin/j_native_a $(OBJS)

#j_native_b: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_B -DJOURNALING main.c -o bin/j_native_b $(OBJS)

#hybrid_a: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_A -DHYBRID main.c -o bin/hybrid_a $(OBJS)

#hybrid_b: $(OBJS) dirs
#	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE_B -DHYBRID main.c -o bin/hybrid_b $(OBJS)

dirs:
	mkdir -p bin

clean:
	rm -rf bin/*