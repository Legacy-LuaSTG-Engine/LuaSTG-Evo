local test = require("test")
local particle = require("particle")
local random = require("random")
local rnd = random.pcg32_fast()
rnd:seed(114514)

-- local particle_pool;


local object_class = {
    function(self)
        self.particle_pool = particle.NewPool2D("img:white", "mul+add", 8192 / 32)
    end,
    function() end,
    function(self)
        local scale = 1.6
        local flicker = lstg.sin(self.timer * 15) * 0.4 + rnd:number(-1, 1)
        for _ = 1, 128 / 32 do
            local pdx = rnd:number(-1, 1) * scale
            local p = self.particle_pool:AddParticle(self.x + flicker * -8, self.y, 0, pdx * 2.1 + flicker * 0.8, rnd:number(-1, 1) * scale, 1 * scale)
            p.ax = -pdx * 0.09
            p.ay = rnd:number(0.06, 0.18) * scale
            p.omiga = rnd:number(-1, 1)
            -- p.color = lstg.Color(0xFF0A22CC)
            -- p.color = lstg.HSVColor(100, rnd:integer(100), 100, 88)
            p.color = lstg.Color(0xFF000000 + bit.lshift(0x77, rnd:integer(2) * 8))
        end
        self.particle_pool:Update()
        -- local amt = 0
        self.particle_pool:Apply(function(p)
            -- amt = amt + 1
            -- if p.timer > 25 and r == 0 then
            --     return true
            -- end
            p.color = p.color * (0.999 * (1 - p.timer * 0.00045))
            if p.timer % 4 ~= 0 then return end
            local r = rnd:integer(13)
            if r == 6 then
                p.vx = p.vx * 0.5
                if p.vx * p.ax > 0 then
                    p.ax = -p.ax
                end
            end
            if r == 0 or (p.y - self.y > 40 * scale and p.ay > 0) then
                p.ay = -p.ay * 0.3
            end
            if p.y - self.y < -8 * scale then
                p.vy = p.vy * 0.3
                if p.ay < 0 then
                    p.ay = -p.ay * 2.1
                end
            end
        end)
    end,
    function(self) self.particle_pool:Render() end,
    function() end,
    function() end;
    is_class = true,
}

---@class test.Module.Texture : test.Base
local M = {}

local obj
function M:onCreate()
    local old_pool = lstg.GetResourceStatus()
    lstg.SetResourceStatus("global")
    lstg.LoadTexture('tex:white', 'res/white.png')
    lstg.LoadImage('img:white', 'tex:white', 0, 0, lstg.GetTextureSize('tex:white'))
    lstg.SetResourceStatus(old_pool)

    obj = lstg.New(object_class)
end

function M:onDestroy()
    lstg.RemoveResource("global", 1, "tex:white")
    lstg.RemoveResource("global", 2, "img:white")
    
    lstg.ResetPool()
end

local timer = 0
local x, y = 800, 400
function M:onUpdate()
    if lstg.Input.Mouse.GetKeyState(lstg.Input.Mouse.Left) then
        obj.x, obj.y = lstg.Input.Mouse.GetPosition()
    end
    lstg.ObjFrame()
    -- timer = timer + 1
    -- print(amt)
end

function M:onRender()
    window:applyCameraV()
    lstg.RenderClear(lstg.Color(0xFF000000))

    -- particle_pool:Render()
    lstg.ObjRender()
end

test.registerTest("test.Module.Particle2D.Object", M)