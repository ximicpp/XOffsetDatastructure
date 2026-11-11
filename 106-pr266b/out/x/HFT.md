# CKPT20 HFT (report only): 64 books, one XBuffer per book, book_c.cpp, ns/event

| impl | access | runs head/anc | head median | anchor median | head/anc | 95% interval |
|---|---|---|---|---|---|---|
| xo_nest | ptr | 24/24 | 35.63 | 37.13 | 0.9597 | [0.9140, 1.0857] |
| xo_nest | root | 24/24 | 38.52 | 37.00 | 1.0411 | [0.9227, 1.0946] |
| xo_pool | ptr | 24/24 | 35.70 | 31.96 | 1.1172 | [0.9972, 1.1258] |
| xo_pool | root | 24/24 | 34.46 | 34.01 | 1.0132 | [0.9265, 1.1315] |

digest / live orders / levels across every run: identical [('4040114569661474', 9976, 1431)]

HFT head/anc: xo_nest/ptr 0.960, xo_nest/root 1.041, xo_pool/ptr 1.117, xo_pool/root 1.013
