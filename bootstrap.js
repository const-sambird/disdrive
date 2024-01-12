const { Client, Events, GatewayIntentBits } = require('discord.js')
const config = require('./config.json')
const discordClient = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.MessageContent, GatewayIntentBits.GuildMessageReactions, GatewayIntentBits.GuildMessages ]})

discordClient.once(Events.ClientReady, c => {
    let str = ''
    for (let i = 0; i < 512; ++i)
        str += '00'
    discordClient.channels.fetch(config.data_channel_id).then(channel => {
        for (let i = 0; i < 100; ++i)
            channel.send({ content: str })
    })
})

discordClient.login(config.discord.token)
