CC = gcc
CFLAGS = -g -w

all:: parallel_cal vector_multiply external_cal parallel_sum_squares matrix_multiply estimating_pi test

parallel_cal:
	$(CC) $(CFLAGS) -pthread -o parallel_cal parallel_cal.c -L../ -lmypthread

estimating_pi:
	$(CC) $(CFLAGS) -pthread -o estimating_pi estimating_pi.c -L../ -lmypthread

parallel_sum_squares:
	$(CC) $(CFLAGS) -pthread -o parallel_sum_squares parallel_sum_squares.c -L../ -lmypthread

vector_multiply:
	$(CC) $(CFLAGS) -pthread -o vector_multiply vector_multiply.c -L../ -lmypthread

matrix_multiply:
	$(CC) $(CFLAGS) -pthread -o matrix_multiply matrix_multiply.c -L../ -lmypthread

external_cal:
	$(CC) $(CFLAGS) -pthread -o external_cal external_cal.c -L../ -lmypthread

test:
	$(CC) $(CFLAGS) -pthread -o test test.c -L../ -lmypthread

clean:
	rm -rf testcase test parallel_cal vector_multiply parallel_sum_squares external_cal estimating_pi *.o ./record/
