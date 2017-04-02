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

struct Prepare {
    uint32_t type;
    uint32_t server_id;
    uint32_t preinstalled;
    uint32_t local_aru;
};

struct Proposal {
    uint32_t server_id;
    uint32_t view;
    uint32_t seq;
    uint32_t update;
    struct Proposal *next;
};

struct Prepare_OK {
    uint32_t type;
    uint32_t server_id;
    uint32_t preinstalled;
    struct Proposal *data_list;
};