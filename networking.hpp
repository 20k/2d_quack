#ifndef NETWORKING_HPP_INCLUDED
#define NETWORKING_HPP_INCLUDED

#include "../2d_quacku_servers/master_server/network_messages.hpp"

udp_sock join_game(const std::string& address, const std::string& port)
{
    udp_sock sock = udp_connect(address, port);

    byte_vector vec;
    vec.push_back(canary_start);
    vec.push_back(message::CLIENTJOINREQUEST);
    vec.push_back(canary_end);

    udp_send(sock, vec.ptr);

    return sock;
}

void leave_game(udp_sock& sock)
{
    sock.close();
}

struct network_state
{
    int my_id = -1;
    udp_sock sock;
    sockaddr_storage store;
    bool have_sock = false;

    std::unordered_map<int32_t, byte_fetch> available_data;

    void tick()
    {
        if(!sock.valid())
            return;

        bool any_recv = true;

        while(any_recv && sock_readable(sock))
        {
            auto data = udp_receive_from(sock, &store);

            any_recv = data.size() > 0;

            byte_fetch fetch;
            fetch.ptr.swap(data);

            //this_frame_stats.bytes_in += data.size();

            while(!fetch.finished() && any_recv)
            {
                int32_t found_canary = fetch.get<int32_t>();

                while(found_canary != canary_start && !fetch.finished())
                {
                    found_canary = fetch.get<int32_t>();
                }

                int32_t type = fetch.get<int32_t>();

                if(type == message::FORWARDING)
                {
                    int32_t player_id = fetch.get<net_type::player_t>();

                    available_data[player_id] = fetch;
                }

                if(type == message::CLIENTJOINACK)
                {
                    int32_t recv_id = fetch.get<int32_t>();

                    int32_t canary_found = fetch.get<int32_t>();

                    if(canary_found == canary_end)
                        my_id = recv_id;
                }
            }
        }
    }

    void join_hardcoded()
    {
        join_game("127.0.0.1", GAMESERVER_PORT);
    }

    void forward_data(int id, byte_vector& vec)
    {
        byte_vector vec;
        vec.push_back(canary_start);
        vec.push_back(message::FORWARDING);
        vec.push_back<net_type::player_t>(id);
        vec.push_vector(vec);
        vec.push_back(canary_end);
    }
};

struct network_serialisable
{
    virtual byte_vector serialise_network() = 0;
    virtual deserialise_network(byte_fetch& fetch) = 0;
};

struct networkable_host : virtual network_serialisable
{
    ///send serialisable properties across network
    virtual void update(networking_state& state)
    {

    }

    virtual void process_recv()
    {

    }
};

struct networkable_client : virtual network_serialisable
{
    ///if we have properties we need to network, but not movement
    ///eg we don't own this
    virtual void update()
    {

    }

    virtual void process_recv()
    {

    }
};

#endif // NETWORKING_HPP_INCLUDED
