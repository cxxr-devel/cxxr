expected <- eval(parse(text="NULL"));  
test(id=0, code={  
argv <- eval(parse(text="list(77, 4L, NULL)"));  
.Internal(`set.seed`(argv[[1]], argv[[2]], argv[[3]]));  
}, o=expected);  

