obj = GetObj()

if ForwordMove(obj, 1, 2, "i") then PlayAni(obj, 1)
elseif ForwordMove(obj, 0, 0, "z") then PlayAni(obj, 2)
else PlayAni(obj, 0) end

ForwordMove(obj, 0, 2, "k")
CharRotation(obj, 0, 1, "l")
CharRotation(obj, 1, 1, "j")