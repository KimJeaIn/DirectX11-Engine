obj = GetObj()

if ForwordMove(obj, 1, 2, "t") then PlayAni(obj, 1)
else PlayAni(obj, 0) end

ForwordMove(obj, 0, 2, "g")
CharRotation(obj, 0, 1, "h")
CharRotation(obj, 1, 1, "f")