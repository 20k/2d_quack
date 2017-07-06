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

struct network_variable
{
    net_type::player_t player_id = -1;
    int16_t object_id = -1;

    network_variable(int32_t in)
    {
        *this = decompose(in);
    }

    network_variable(int16_t pid, int16_t oid)
    {
        player_id = pid;
        object_id = oid;
    }

    network_variable(){}

    network_variable decompose(int32_t in) const
    {
        network_variable ret;

        ret.player_id = (in >> 16) & 0xFFFF;
        ret.object_id = in & 0xFFFF;

        return ret;
    }

    int32_t compose()
    {
        return (player_id << 16) | object_id;
    }
};

struct network_state
{
    int my_id = -1;
    int16_t next_object_id = 0;
    udp_sock sock;
    sockaddr_storage store;
    bool have_sock = false;

   std::vector<std::pair<network_variable, byte_fetch>> available_data;

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
                    //int32_t player_id = fetch.get<net_type::player_t>();

                    network_variable nv = fetch.get<network_variable>();

                    //available_data[player_id] = fetch;
                    available_data.push_back({nv, fetch});
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

    void forward_data(int player_id, int object_id, const byte_vector& vec)
    {
        network_variable nv(player_id, object_id);

        byte_vector cv;
        cv.push_back(canary_start);
        cv.push_back(message::FORWARDING);
        cv.push_back<network_variable>(nv);
        cv.push_vector(vec);
        cv.push_back(canary_end);

        udp_send_to(sock, cv.ptr, (const sockaddr*)&store);
    }

    int16_t get_next_object_id()
    {
        return next_object_id++;
    }
};

struct network_serialisable
{
    int my_id = -1;

    virtual byte_vector serialise_network() = 0;
    virtual deserialise_network(byte_fetch& fetch) = 0;

    virtual void update(network_state& state) = 0;
    virtual void process_recv(network_state& state) = 0;
};

struct networkable_host : virtual network_serialisable
{
    int object_id = -1;

    networkable_host(network_state& ns)
    {
        object_id = ns.get_next_object_id();
        my_id = ns.my_id;
    }

    ///send serialisable properties across network
    virtual void update(network_state& ns)
    {
        ns.forward_data(my_id, object_id, serialise_network());
    }

    virtual void process_recv(network_state& ns)
    {
        for(auto& i : ns.available_data)
        {
            network_variable& var = i.first;

            if(var.player_id == my_id && var.object_id == object_id)
            {
                deserialise_network(i.second);
            }
        }
    }
};

///we need to discover new objects as they're sent to us
struct networkable_client : virtual network_serialisable
{
    int object_id = -1;

    ///if we have properties we need to network, but not movement
    ///eg we don't own this
    virtual void update(network_state& ns)
    {

    }

    virtual void process_recv(network_state& ns)
    {

    }
};

#endif // NETWORKING_HPP_INCLUDED
