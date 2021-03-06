#pragma once

#include "application.hpp"
#include "fsm.hpp"

namespace lmail
{

class CliState;
class Cli final : public Fsm<CliState>
{
public:
    explicit Cli(Application::Conf const &conf);
    void run();

private:
    void on_state_changed() override;

private:
    Application::Conf conf_;
};

} // namespace lmail
