//
// Created by chris on 28/05/2020.
//


/*  Stride statistics
 *  arr[x] -> x + N1
 *      |  -> X + N2
 *      |  -> X + N3
 */

// Initial code
int main() {
    while (cond) {
        int x = non_predictable.next();
        y = array[x];
        ...
    }
}

// Optimized code
int main() {
    while (cond) {
        prefetch(&array[x + N1])
        prefetch(&array[x + N2])
        prefetch(&array[x + N3])
        int x = non_predictable.next();
        y = array[x];
        ...
    }
}
