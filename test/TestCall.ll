; ModuleID = 'TestCall.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @foo(i8* %arg) nounwind uwtable {
entry:
  ret void
}

define i32 @main() nounwind uwtable {
entry:
  %p = call noalias i8* @malloc(i64 1024) nounwind
  call void @foo(i8* %p)
  %q = bitcast i8* %p to i32*
  ret i32 0
}

declare noalias i8* @malloc(i64) nounwind
