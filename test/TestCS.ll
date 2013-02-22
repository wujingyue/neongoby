; ModuleID = 'TestCS.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @foo(i8* %a, i32 %cont) nounwind uwtable {
entry:
  %tobool = icmp ne i32 %cont, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %b = call noalias i8* @malloc(i64 1024) nounwind
  call void @foo(i8* %b, i32 0)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret void
}

declare noalias i8* @malloc(i64) nounwind

define i32 @main() nounwind uwtable {
entry:
  call void @foo(i8* null, i32 1)
  ret i32 0
}
