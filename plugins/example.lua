-- example TEdit plugin
-- place in %APPDATA%\tedit\plugins\

-- this plugin adds a simple greeting command (cuz ur goated for getting this program)

tedit_echo("Example plugin loaded!")

-- you can define custom functions that interact with the editor:
function greet()
tedit_echo("Hello from the example plugin!")
tedit_echo("Current buffer has " .. tedit_line_count() .. " lines")
end

-- to use ts: run 'lua greet()' in tedit
