# CKPT19 HFT (report only): 64 books, one XBuffer per book, book_c.cpp, ns/event

| impl | access | runs head/anc | head median | anchor median | head/anc | 95% interval |
|---|---|---|---|---|---|---|
| xo_nest | ptr | 24/24 | 33.25 | 39.77 | 0.8362 | [0.8300, 0.9172] |
| xo_nest | root | 24/24 | 37.11 | 37.22 | 0.9968 | [0.8958, 1.0098] |
| xo_pool | ptr | 24/24 | 31.14 | 33.34 | 0.9339 | [0.8402, 1.0464] |
| xo_pool | root | 24/24 | 31.03 | 36.38 | 0.8529 | [0.8476, 0.9572] |

digest / live orders / levels across every run: identical [('4040114569661474', 9976, 1431)]

HFT head/anc: xo_nest/ptr 0.836, xo_nest/root 0.997, xo_pool/ptr 0.934, xo_pool/root 0.853
