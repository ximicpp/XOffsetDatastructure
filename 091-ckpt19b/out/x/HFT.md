# CKPT19 HFT (report only): 64 books, one XBuffer per book, book_c.cpp, ns/event

| impl | access | runs head/anc | head median | anchor median | head/anc | 95% interval |
|---|---|---|---|---|---|---|
| xo_nest | ptr | 24/24 | 36.12 | 36.58 | 0.9876 | [0.8775, 0.9970] |
| xo_nest | root | 24/24 | 36.25 | 37.54 | 0.9655 | [0.8726, 1.0021] |
| xo_pool | ptr | 24/24 | 33.25 | 33.53 | 0.9915 | [0.8397, 1.0483] |
| xo_pool | root | 24/24 | 34.38 | 32.75 | 1.0498 | [0.9317, 1.0693] |

digest / live orders / levels across every run: identical [('4040114569661474', 9976, 1431)]

HFT head/anc: xo_nest/ptr 0.988, xo_nest/root 0.966, xo_pool/ptr 0.992, xo_pool/root 1.050
