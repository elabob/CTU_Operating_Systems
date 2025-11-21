//
// Created by bobenade on 21/11/2025.
//

typedef unsigned int uint;
typedef long ssize_t;
typedef unsigned long long ull;

// systemove volania pre 32bit linux
static ssize_t my_read(int fd, void *buf, unsigned n) {
    int r;
    asm volatile(
        "int $0x80"
        : "=a"(r)
        : "a"(3), "b"(fd), "c"(buf), "d"(n)
    );
    return r;
}

static ssize_t my_write(int fd, const void *buf, unsigned n) {
    int r;
    asm volatile(
        "int $0x80"
        : "=a"(r)
        : "a"(4), "b"(fd), "c"(buf), "d"(n)
    );
    return r;
}

static void my_exit(int code) {
    asm volatile(
        "int $0x80"
        :
        : "a"(1), "b"(code)
    );
    for (;;) {}
}

// kontrola ci je znak cislica
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

// prevedie uint na hex zapis do buffra
static int to_hex(uint x, char *out) {
    char tmp[8];
    int p = 0;

    // specialny pripad pre 0 :)
    if (x == 0) {
        tmp[p++] = '0';
    } else {
        // hex cifry odzadu
        while (x > 0) {
            uint d = x & 0xF;
            if (d < 10) tmp[p++] = '0' + d;
            else tmp[p++] = 'a' + (d - 10);
            x >>= 4;
        }
    }

    int k = 0;
    out[k++] = '0';
    out[k++] = 'x';

    // reverse - vypisem cifry v spravnom poradi
    for (int i = p - 1; i >= 0; i--) {
        out[k++] = tmp[i];
    }

    out[k++] = '\n';
    return k;
}

void _start() {
    char buf[4096];
    ssize_t n;

    ull acc = 0;
    int reading = 0;      // ci som v stave citania cisla
    int seen_digit = 0;   // ci som uz predtym videla aspon jednu cislicu

    while (1) {
        n = my_read(0, buf, sizeof(buf));
        if (n < 0) my_exit(1);
        if (n == 0) break;

        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];

            if (is_digit(c)) {
                acc = acc * 10 + (c - '0');
                reading = 1;
                seen_digit = 1;
            } else {
                if (reading && seen_digit) {
                    // ukoncujem cislo
                    char out[32];
                    int len = to_hex((uint)acc, out);
                    my_write(1, out, len);

                    acc = 0;
                    reading = 0;
                    seen_digit = 0;
                }
            }
        }
    }

    // EOF - ak som ostal v stave citania cisla, vypis ho
    if (reading && seen_digit) {
        char out[32];
        int len = to_hex((uint)acc, out);
        my_write(1, out, len);
    }

    my_exit(0);
}