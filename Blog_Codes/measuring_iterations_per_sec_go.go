// Measuring thread switching time using a UNIX pipe.
//
// Sanket [https://github.com/san-gg]
// This code is in the public domain.

package main

import (
	"fmt"
	"time"

	"golang.org/x/sys/unix"
)

func child(ch chan string) {
	c := 0
	for msg := range ch {
		c++
		if len(msg) != 4 {
			panic("unexpected message length")
		}
		ch <- msg
	}
	fmt.Println(c, "messages processed by child")
	ch <- "done"
}

func main() {
	ch := make(chan string)
	go child(ch)

	t1 := time.Now()
	const niters = 200000
	for i := 0; i < niters; i++ {
		ch <- "abcd"
		reply := <-ch
		if "abcd" != reply {
			panic("mismatch reply")
		}
	}
	elapsed := time.Since(t1)
	close(ch)

	iters_per_sec := 1e6*niters / float64(elapsed.Microseconds())
	fmt.Println(niters, "iterations took", elapsed.Microseconds(), "us.", iters_per_sec, "iters/sec")

	var ru unix.Rusage
	if err := unix.Getrusage(unix.RUSAGE_SELF, &ru); err != nil {
		panic(err)
	}

	fmt.Println("From getrusage:")
	fmt.Printf("  voluntary switches = %d\n", ru.Nvcsw)
	fmt.Printf("  involuntary switches = %d\n", ru.Nivcsw)

}
