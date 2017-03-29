struct View_Change {
    uint32_t type;          // must equal to 2
    uint32_t server_id;     // unique id of the sending server
    uint32_t attempted;     // view number this server is trying to install
};

struct VC_Proof {
    uint32_t type;          // must equal to 3
    uint32_t server_id;     // unique id of the sending server
    uint32_t installed;     // last view number this server installed
};