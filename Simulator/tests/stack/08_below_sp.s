; EXPECT: ACCESS_BELOW_SP
sub r13, r13, 8
add r13, r13, 8
st r1, -4[r13]
halt
