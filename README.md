# cpp-datetime-examples


对于以上代码，在不使用 tcmalloc（将__tcmalloc 标志去掉）时，运行时间需要 10000 多毫秒（实测 14846），而在使用 tcmalloc 后，运行时间仅需 100 多毫秒（实测 151）。


不用 tcmalloc 更快，伤心了