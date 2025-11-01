print("main.lua loaded")

function on_reload()
  print("on_reload called")
end

function update(dt)
  cpp_print("update dt = "..tostring(dt))
end