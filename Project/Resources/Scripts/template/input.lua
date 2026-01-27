local Component = require("mono_helper")

return Component {
    fields = {
        name = "PlayerController",
        speed = 5.0,
        jumpForce = 10.0,
        position = { x = 0, y = 0, z = 0 }
    },
    
    Awake = function(self)
        print("PlayerController Awake - Input system ready!")
    end,
    
    Start = function(self)
        print("PlayerController Started")
        print("Use WASD to move, Space to jump, Mouse to look around")
    end,
    
    Update = function(self, dt)
        -- Keyboard movement - Use GetKey() function with Key constants
        if Input.GetKey(Input.Key.W) then
            print("W key held - Moving forward")
            self.position.z = self.position.z + (dt * self.speed)
        end
        
        if Input.GetKey(Input.Key.S) then
            print("S key held - Moving backward")
            self.position.z = self.position.z - (dt * self.speed)
        end
        
        if Input.GetKey(Input.Key.A) then
            print("A key held - Moving left")
            self.position.x = self.position.x - (dt * self.speed)
        end
        
        if Input.GetKey(Input.Key.D) then
            print("D key held - Moving right")
            self.position.x = self.position.x + (dt * self.speed)
        end
        
        -- Jump with Space (only triggers once per press)
        if Input.GetKeyDown(Input.Key.Space) then
            print("Space pressed - JUMP!")
            self.position.y = self.position.y + self.jumpForce
        end
        
        -- Mouse button detection
        if Input.GetMouseButtonDown(Input.MouseButton.Left) then
            local mouseX = Input.GetMouseX()
            local mouseY = Input.GetMouseY()
            print("Left mouse clicked at: (" .. mouseX .. ", " .. mouseY .. ")")
        end
        
        if Input.GetMouseButton(Input.MouseButton.Right) then
            print("Right mouse button held")
        end
        
        -- Check for any input
        if Input.GetAnyKeyDown() then
            print("Some key was just pressed!")
        end
        
        -- F keys for debug
        if Input.GetKeyDown(Input.Key.F1) then
            print("F1 pressed - Position: x=" .. self.position.x .. 
                  " y=" .. self.position.y .. " z=" .. self.position.z)
        end
    end,
    
    OnDisable = function(self)
        print("PlayerController disabled - Final position: x=" .. 
              self.position.x .. " y=" .. self.position.y .. " z=" .. self.position.z)
    end
}