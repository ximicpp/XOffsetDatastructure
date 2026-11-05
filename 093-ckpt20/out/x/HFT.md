# CKPT20 HFT (report only): 64 books, one XBuffer per book, book_c.cpp, ns/event

| impl | access | runs head/anc | head median | anchor median | head/anc | 95% interval |
|---|---|---|---|---|---|---|
| xo_nest | ptr | 24/24 | 36.91 | 37.72 | 0.9785 | [0.9202, 1.0946] |
| xo_nest | root | 24/24 | 40.00 | 40.11 | 0.9973 | [0.9160, 1.0934] |
| xo_pool | ptr | 24/24 | 33.84 | 35.58 | 0.9514 | [0.8931, 1.1128] |
| xo_pool | root | 24/24 | 35.61 | 35.34 | 1.0075 | [0.9136, 1.1125] |

digest / live orders / levels across every run: identical [('4040114569661474', 9976, 1431)]

HFT head/anc: xo_nest/ptr 0.979, xo_nest/root 0.997, xo_pool/ptr 0.951, xo_pool/root 1.007
