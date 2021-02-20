# Ukkonen suffix tree
My implementation of ukkonen's suffix tree creation algorithm.
Based on this [stackoverflow post](https://stackoverflow.com/questions/9452701/ukkonens-suffix-tree-algorithm-in-plain-english/9513423#9513423).

I also added visualization of all the steps via graphviz.
For each step a description of the tree 
in a .dot format is created.
To get a viewable image you need to feed the `step_xxx.dot`
files into `dot` program.

The command to produce png files is `dot -Tpng step_xxx.dot -o step_xxx.png`
