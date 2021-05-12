#!/usr/bin/python3
from texttable import Texttable
t = Texttable()
t.add_rows([['Name', 'Age'], ['Alice'*100, "a"*100], ['Bob', 19]])
print(t.draw())
