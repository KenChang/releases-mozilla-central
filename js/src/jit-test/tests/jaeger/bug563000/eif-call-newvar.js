// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  assertJit();
  evalInFrame(1, "var x = 'success'");
}
function caller() {
  eval();
  callee();
  return x;
}
assertEq(caller(), "success");
assertEq(typeof x, "undefined");
