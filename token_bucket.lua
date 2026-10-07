local key=KEYS[1]
local capacity=tonumber(ARGV[1]);
local rate=tonumber(ARGV[2]);
local requested=tonumber(ARGV[3]);

local t=redis.call('TIME')

local now=tonumber(t[1])+tonumber(t[2])/1000000

local data = redis.call('HMGET',KEYS[1],'tokens','ts')

local tokens=tonumber(data[1]) -- available tokens when bucket was updated
local ts=tonumber(data[2]) -- time stemp when bucket was updated

if tokens==nil then
    tokens=capacity
    ts=now
end

local elapsed = math.max(0,now-ts)
tokens=math.min(capacity,tokens + elapsed*rate)

local allowed=0
if tokens>= requested then
    tokens=tokens-requested
    allowed=1
end

redis.call('HSET',key,'tokens',tokens,'ts',now)
redis.call('Expire',key,math.ceil(capacity/rate)*2)

return allowed